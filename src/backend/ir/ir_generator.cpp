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
#include <cassert>

namespace aria {
using namespace sema;  // Now inside aria namespace

IRGenerator::IRGenerator(const std::string& module_name, bool enable_debug)
    : context(), 
      module(std::make_unique<llvm::Module>(module_name.empty() ? "aria_module" : module_name, context)),
      builder(context),
      type_system(nullptr),
      di_builder(nullptr),
      di_compile_unit(nullptr),
      di_file(nullptr),
      debug_enabled(enable_debug),
      tbb_codegen(context, builder),
      ternary_codegen(context, builder) {
    // Set source filename for better debug info
    module->setSourceFileName(module_name.empty() ? "aria_module" : module_name);
    
    if (debug_enabled) {
        // Create DIBuilder (initialization deferred to initDebugInfo)
        di_builder = std::make_unique<llvm::DIBuilder>(*module);
    }
}

void IRGenerator::setTypeSystem(TypeSystem* ts) {
    type_system = ts;
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
            auto* ptr_type = static_cast<sema::PointerType*>(aria_type);
            llvm::Type* pointee = mapType(ptr_type->getPointeeType());
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
                builder.getInt1Ty()                 // is_error (field 2) - bool
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

// Helper function to map type name strings to LLVM types
llvm::Type* IRGenerator::mapTypeFromName(const std::string& type_name) {
    // Check for void
    if (type_name == "void") {
        return builder.getVoidTy();
    }
    
    // Check for optional types (e.g., "int64?", "string?")
    // Optional types are represented as structs: { i1 hasValue, T value }
    if (type_name.size() >= 2 && type_name.back() == '?') {
        std::string wrapped_type = type_name.substr(0, type_name.size() - 1);
        llvm::Type* valueType = mapTypeFromName(wrapped_type);
        return llvm::StructType::get(context, {builder.getInt1Ty(), valueType});
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
    
    // Floating point types
    if (type_name == "flt16") return builder.getHalfTy();
    if (type_name == "flt32") return builder.getFloatTy();
    if (type_name == "flt64") return builder.getDoubleTy();
    if (type_name == "flt128") return llvm::Type::getFP128Ty(context);
    // Extended precision floats (library-based, represented as integer bits)
    if (type_name == "flt256") return builder.getIntNTy(256);  // Placeholder: needs library support
    if (type_name == "flt512") return builder.getIntNTy(512);  // Placeholder: needs library support
    
    // Integer types - signed
    if (type_name == "int1") return builder.getInt1Ty();
    if (type_name == "int2") return builder.getIntNTy(2);
    if (type_name == "int4") return builder.getIntNTy(4);
    if (type_name == "int8") return builder.getInt8Ty();
    if (type_name == "int16") return builder.getInt16Ty();
    if (type_name == "int32") return builder.getInt32Ty();
    if (type_name == "int64") return builder.getInt64Ty();
    if (type_name == "int128") return builder.getInt128Ty();
    if (type_name == "int256") return builder.getIntNTy(256);
    if (type_name == "int512") return builder.getIntNTy(512);
    
    // Integer types - unsigned (same representation in LLVM)
    if (type_name == "uint1") return builder.getInt1Ty();
    if (type_name == "uint2") return builder.getIntNTy(2);
    if (type_name == "uint4") return builder.getIntNTy(4);
    if (type_name == "uint8") return builder.getInt8Ty();
    if (type_name == "uint16") return builder.getInt16Ty();
    if (type_name == "uint32") return builder.getInt32Ty();
    if (type_name == "uint64") return builder.getInt64Ty();
    if (type_name == "uint128") return builder.getInt128Ty();
    if (type_name == "uint256") return builder.getIntNTy(256);
    if (type_name == "uint512") return builder.getIntNTy(512);
    
    // TBB types - Ternary Balanced Binary (symmetric signed integers with ERR sentinel)
    if (type_name == "tbb8") return builder.getInt8Ty();
    if (type_name == "tbb16") return builder.getInt16Ty();
    if (type_name == "tbb32") return builder.getInt32Ty();
    if (type_name == "tbb64") return builder.getInt64Ty();
    
    // Balanced Ternary types
    if (type_name == "trit") return builder.getIntNTy(3);   // Single trit (-1, 0, 1) in 3 bits for overflow
    if (type_name == "tryte") return builder.getInt16Ty();  // 10 trits in 16 bits
    
    // Nonary types  
    if (type_name == "nit") return builder.getIntNTy(4);    // Single nit (-4 to 4) in 4 bits
    if (type_name == "nyte") return builder.getInt16Ty();   // 5 nits in 16 bits
    
    // String type - represented as i8* (pointer to null-terminated char array)
    if (type_name == "string") {
        return llvm::PointerType::get(context, 0);
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
        // vec9 / dvec9 with flt64 components (struct of 9 doubles)
        std::vector<llvm::Type*> components(9, builder.getDoubleTy());
        return llvm::StructType::get(context, components);
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
    
    // Check for custom types (structs, unions, etc.) if TypeSystem is available
    if (type_system) {
        // Look up struct type
        Type* aria_type = type_system->getStructType(type_name);
        if (aria_type) {
            return mapType(aria_type);
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
    
    for (const auto* spec : specializations) {
        if (!spec || !spec->funcDecl) {
            continue;
        }
        
        // Generate function signature
        FuncDeclStmt* funcDecl = spec->funcDecl;
        
        // Determine return type using proper type mapping
        std::string returnTypeStr = funcDecl->returnType ? funcDecl->returnType->toString() : "void";
        llvm::Type* returnType = mapTypeFromName(returnTypeStr);
        
        // Build parameter types with proper type mapping
        std::vector<llvm::Type*> paramTypes;
        for (const auto& param : funcDecl->parameters) {
            ParameterNode* pnode = static_cast<ParameterNode*>(param.get());
            std::string paramTypeStr = pnode->typeNode ? pnode->typeNode->toString() : "void";
            paramTypes.push_back(mapTypeFromName(paramTypeStr));
        }
        
        // Create function type
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            returnType,
            paramTypes,
            false  // not vararg
        );
        
        // Create function with mangled name
        llvm::Function* func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            spec->mangledName,
            module.get()
        );
        
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
                named_values[param->paramName] = &arg;
            }
            idx++;
        }
        
        // Generate function body
        if (funcDecl->body) {
            llvm::Value* bodyVal = codegenStatement(funcDecl->body.get());
            
            // If body didn't already create a terminator, add one
            if (!builder.GetInsertBlock()->getTerminator()) {
                if (returnType->isVoidTy()) {
                    builder.CreateRetVoid();
                } else {
                    // Default return value
                    builder.CreateRet(llvm::ConstantInt::get(returnType, 0));
                }
            }
        } else {
            // No body - create default return
            if (returnType->isVoidTy()) {
                builder.CreateRetVoid();
            } else {
                builder.CreateRet(llvm::ConstantInt::get(returnType, 0));
            }
        }
        
        // Clear parameter names from symbol table
        for (const auto& param : funcDecl->parameters) {
            ParameterNode* pnode = static_cast<ParameterNode*>(param.get());
            named_values.erase(pnode->paramName);
        }
        
        generated++;
    }
    
    return generated;
}

// =============================================================================
// Module Declaration Processing (Recursive)
// =============================================================================

void aria::IRGenerator::processModuleDeclarations(const std::vector<std::shared_ptr<ASTNode>>& declarations,
                                                    const std::string& modulePrefix) {
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
        
        // Handle FUNC_DECL statements
        if (decl->type == ASTNode::NodeType::FUNC_DECL) {
            FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(decl.get());
            
            // Skip generic functions (handled by monomorphization)
            if (!funcDecl->genericParams.empty()) {
                continue;
            }
            
            // Create function signature with proper type mapping
            std::vector<llvm::Type*> param_types;
            for (const auto& param : funcDecl->parameters) {
                ParameterNode* pnode = static_cast<ParameterNode*>(param.get());
                std::string paramTypeStr = pnode->typeNode ? pnode->typeNode->toString() : "void";
                param_types.push_back(mapTypeFromName(paramTypeStr));
            }
            
            std::string returnTypeStr = funcDecl->returnType ? funcDecl->returnType->toString() : "void";
            llvm::Type* return_type = mapTypeFromName(returnTypeStr);
            
            // Phase 4.6: Async functions return i8* (coroutine handle)
            llvm::Type* actual_return_type = return_type;
            if (funcDecl->isAsync) {
                actual_return_type = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
                std::cerr << "[DEBUG] Async function detected: " << funcDecl->funcName << ", changing return type to i8*" << std::endl;
            }
            
            llvm::FunctionType* func_type = llvm::FunctionType::get(actual_return_type, param_types, false);
            
            // Determine function name (qualified if in a module)
            std::string func_name = modulePrefix.empty() 
                ? funcDecl->funcName 
                : modulePrefix + "." + funcDecl->funcName;
            
            // Determine linkage based on pub modifier
            // Special case: main function always has external linkage
            llvm::Function::LinkageTypes linkage;
            if (funcDecl->funcName == "main") {
                linkage = llvm::Function::ExternalLinkage;
            } else {
                linkage = funcDecl->isPublic 
                    ? llvm::Function::ExternalLinkage 
                    : llvm::Function::InternalLinkage;
            }
            
            // Create LLVM function
            llvm::Function* func = llvm::Function::Create(
                func_type,
                linkage,
                func_name,
                module.get()
            );
            
            // Skip body generation if no body (extern declaration)
            if (!funcDecl->body) {
                continue;
            }
            
            // Create entry block
            llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
            builder.SetInsertPoint(entry);
            
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
            }
            
            // Phase 4.6: Async function coroutine setup
            llvm::Value* coro_id = nullptr;
            llvm::Value* coro_handle = nullptr;
            llvm::BasicBlock* coro_suspend_block = nullptr;
            llvm::BasicBlock* coro_cleanup_block = nullptr;
            
            if (funcDecl->isAsync) {
                // 1. Generate coroutine ID: token @llvm.coro.id(i32 align, i8* promise, i8* coroaddr, i8* fnaddr)
                llvm::Function* coroIdFunc = llvm::Intrinsic::getDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_id
                );
                
                llvm::Value* align = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 8);
                llvm::Value* null_ptr = llvm::ConstantPointerNull::get(
                    llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)
                );
                
                coro_id = builder.CreateCall(
                    coroIdFunc,
                    {align, null_ptr, null_ptr, null_ptr},
                    "coro.id"
                );
                
                // 2. Get coroutine frame size
                llvm::Function* coroSizeFunc = llvm::Intrinsic::getDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_size,
                    {llvm::Type::getInt64Ty(context)}
                );
                llvm::Value* coro_size = builder.CreateCall(coroSizeFunc, {}, "coro.size");
                
                // 3. Allocate coroutine frame
                llvm::Function* malloc_func = module->getFunction("malloc");
                if (!malloc_func) {
                    llvm::FunctionType* malloc_type = llvm::FunctionType::get(
                        llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                        {llvm::Type::getInt64Ty(context)},
                        false
                    );
                    malloc_func = llvm::Function::Create(
                        malloc_type,
                        llvm::Function::ExternalLinkage,
                        "malloc",
                        module.get()
                    );
                }
                llvm::Value* coro_mem = builder.CreateCall(malloc_func, {coro_size}, "coro.alloc");
                
                // 4. Begin coroutine
                llvm::Function* coroBeginFunc = llvm::Intrinsic::getDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_begin
                );
                coro_handle = builder.CreateCall(coroBeginFunc, {coro_id, coro_mem}, "coro.handle");
                
                // Create suspend and cleanup blocks for later use
                coro_suspend_block = llvm::BasicBlock::Create(context, "coro.suspend", func);
                coro_cleanup_block = llvm::BasicBlock::Create(context, "coro.cleanup", func);
            }
            
            // Map parameters to symbol table
            size_t idx = 0;
            for (auto& arg : func->args()) {
                if (idx < funcDecl->parameters.size()) {
                    ParameterNode* param = static_cast<ParameterNode*>(funcDecl->parameters[idx].get());
                    arg.setName(param->paramName);
                    named_values[param->paramName] = &arg;
                }
                idx++;
            }
            
            // Generate function body
            std::cerr << "[DEBUG] Generating body for function: " << func_name << std::endl;
            llvm::Value* bodyVal = codegenStatement(funcDecl->body.get());
            std::cerr << "[DEBUG] Body generation complete for: " << func_name << std::endl;
            (void)bodyVal;  // Suppress unused warning
            
            // Add terminator if missing
            if (!builder.GetInsertBlock()->getTerminator()) {
                if (funcDecl->isAsync) {
                    // Async function: Jump to final suspend instead of returning directly
                    builder.CreateBr(coro_suspend_block);
                } else if (return_type->isVoidTy()) {
                    builder.CreateRetVoid();
                } else {
                    builder.CreateRet(llvm::ConstantInt::get(return_type, 0));
                }
            }
            
            // Phase 4.6: Generate coroutine suspend and cleanup blocks for async functions
            if (funcDecl->isAsync) {
                // Final suspend point (where all returns converge)
                builder.SetInsertPoint(coro_suspend_block);
                
                // Save coroutine state before final suspend
                llvm::Function* coroSaveFunc = llvm::Intrinsic::getDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_save
                );
                llvm::Value* save_token = builder.CreateCall(coroSaveFunc, {coro_handle}, "coro.save");
                
                // Final suspend: i8 @llvm.coro.suspend(token save, i1 final)
                llvm::Function* coroSuspendFunc = llvm::Intrinsic::getDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_suspend
                );
                llvm::Value* is_final = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1);
                llvm::Value* suspend_result = builder.CreateCall(
                    coroSuspendFunc,
                    {save_token, is_final},
                    "coro.suspend.result"
                );
                
                // Switch on suspend result
                llvm::SwitchInst* suspend_switch = builder.CreateSwitch(suspend_result, coro_cleanup_block, 1);
                
                // Cleanup block: free coroutine frame
                builder.SetInsertPoint(coro_cleanup_block);
                
                // Get memory to free
                llvm::Function* coroFreeFunc = llvm::Intrinsic::getDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_free
                );
                llvm::Value* free_mem = builder.CreateCall(coroFreeFunc, {coro_id, coro_handle}, "coro.free.mem");
                
                // Free the memory
                llvm::Function* free_func = module->getFunction("free");
                if (!free_func) {
                    llvm::FunctionType* free_type = llvm::FunctionType::get(
                        llvm::Type::getVoidTy(context),
                        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)},
                        false
                    );
                    free_func = llvm::Function::Create(
                        free_type,
                        llvm::Function::ExternalLinkage,
                        "free",
                        module.get()
                    );
                }
                builder.CreateCall(free_func, {free_mem});
                
                // End coroutine
                llvm::Function* coroEndFunc = llvm::Intrinsic::getDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_end
                );
                llvm::Value* unwind = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
                builder.CreateCall(coroEndFunc, {coro_handle, unwind});
                
                // Return coroutine handle
                builder.CreateRet(coro_handle);
            }
            
            // Clean up symbol table
            for (const auto& param : funcDecl->parameters) {
                ParameterNode* pnode = static_cast<ParameterNode*>(param.get());
                named_values.erase(pnode->paramName);
            }
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
    
    switch (stmt->type) {
        case ASTNode::NodeType::BLOCK: {
            // Block statement - generate code for each statement
            BlockStmt* block = static_cast<BlockStmt*>(stmt);
            
            // Push new defer scope for this block
            defer_stack.push_back(std::vector<BlockStmt*>());
            
            llvm::Value* lastVal = nullptr;
            for (const auto& s : block->statements) {
                lastVal = codegenStatement(s.get());
            }
            
            // Execute defers at block exit (LIFO order)
            executeScopeDefers();
            
            // Pop defer scope
            defer_stack.pop_back();
            
            return lastVal;
        }
        
        case ASTNode::NodeType::RETURN: {
            // Return statement - execute all defers before returning
            ReturnStmt* ret = static_cast<ReturnStmt*>(stmt);
            
            // Execute all defer blocks (LIFO order)
            executeFunctionDefers();
            
            if (ret->value) {
                llvm::Value* retVal = codegenExpression(ret->value.get());
                if (retVal) {
                    builder.CreateRet(retVal);
                }
            } else {
                builder.CreateRetVoid();
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
            
            // llvm::errs() << "DEBUG: Mapped to LLVM type: ";
            // varType->print(llvm::errs());
            // llvm::errs() << "\n";
            
            llvm::AllocaInst* alloca = builder.CreateAlloca(varType, nullptr, varDecl->varName);
            
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
                } else {
                    // Not a vector, try struct then primitive
                    aria_type = type_system->getStructType(actualTypeName);
                    if (!aria_type) {
                        aria_type = type_system->getPrimitiveType(actualTypeName);
                    }
                }
                
                if (aria_type) {
                    value_types[alloca] = aria_type;
                }
            }
            
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
                        if (varDecl->initializer->type == ASTNode::NodeType::IDENTIFIER) {
                            IdentifierExpr* idExpr = static_cast<IdentifierExpr*>(varDecl->initializer.get());
                            isNil = (idExpr->name == "NIL");
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
                            // For integer types, use trunc or sext/zext
                            if (initVal->getType()->isIntegerTy() && varType->isIntegerTy()) {
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
                        builder.CreateStore(initVal, alloca);
                    }
                }
            } else if (isOptional) {
                // No initializer for optional type - default to NIL (None)
                llvm::Value* noneValue = createOptionalNone(varType);
                builder.CreateStore(noneValue, alloca);
            }
            
            // Add to symbol table
            named_values[varDecl->varName] = alloca;
            return alloca;
        }
        
        case ASTNode::NodeType::IF: {
            // If statement with optional else
            IfStmt* ifStmt = static_cast<IfStmt*>(stmt);
            
            llvm::Value* condVal = codegenExpression(ifStmt->condition.get());
            if (!condVal) return nullptr;
            
            // Convert condition to bool by comparing to zero
            condVal = builder.CreateICmpNE(condVal, llvm::ConstantInt::get(condVal->getType(), 0), "ifcond");
            
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
            condVal = builder.CreateICmpNE(condVal, llvm::ConstantInt::get(condVal->getType(), 0), "whilecond");
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
            if (forStmt->update) {
                codegenStatement(forStmt->update.get());
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
        
        case ASTNode::NodeType::PASS: {
            // Pass statement - return success with value
            PassStmt* passStmt = static_cast<PassStmt*>(stmt);
            
            // Execute all defer blocks (LIFO order) before returning
            executeFunctionDefers();
            
            // Generate code for the value expression
            llvm::Value* value = codegenExpression(passStmt->value.get());
            if (value) {
                // Convert value to match function return type if necessary (Session 15: ternary support)
                llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();
                llvm::Type* returnType = currentFunc->getReturnType();
                
                if (value->getType() != returnType) {
                    // Handle integer conversions
                    if (value->getType()->isIntegerTy() && returnType->isIntegerTy()) {
                        llvm::IntegerType* valIntTy = llvm::cast<llvm::IntegerType>(value->getType());
                        llvm::IntegerType* retIntTy = llvm::cast<llvm::IntegerType>(returnType);
                        
                        if (valIntTy->getBitWidth() < retIntTy->getBitWidth()) {
                            // Sign extend for widening
                            value = builder.CreateSExt(value, returnType, "ret_sext");
                        } else if (valIntTy->getBitWidth() > retIntTy->getBitWidth()) {
                            // Truncate for narrowing
                            value = builder.CreateTrunc(value, returnType, "ret_trunc");
                        }
                    }
                }
                
                builder.CreateRet(value);
            }
            return nullptr;
        }
        
        case ASTNode::NodeType::FAIL: {
            // Fail statement - return failure with error code
            FailStmt* failStmt = static_cast<FailStmt*>(stmt);
            
            // Execute all defer blocks (LIFO order) before returning
            executeFunctionDefers();
            
            // Generate code for the error code expression
            llvm::Value* errorCode = codegenExpression(failStmt->errorCode.get());
            if (errorCode) {
                // Convert error code to match function return type if necessary (Session 15: ternary support)
                llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();
                llvm::Type* returnType = currentFunc->getReturnType();
                
                if (errorCode->getType() != returnType) {
                    // Handle integer conversions
                    if (errorCode->getType()->isIntegerTy() && returnType->isIntegerTy()) {
                        llvm::IntegerType* valIntTy = llvm::cast<llvm::IntegerType>(errorCode->getType());
                        llvm::IntegerType* retIntTy = llvm::cast<llvm::IntegerType>(returnType);
                        
                        if (valIntTy->getBitWidth() < retIntTy->getBitWidth()) {
                            // Sign extend for widening
                            errorCode = builder.CreateSExt(errorCode, returnType, "ret_sext");
                        } else if (valIntTy->getBitWidth() > retIntTy->getBitWidth()) {
                            // Truncate for narrowing
                            errorCode = builder.CreateTrunc(errorCode, returnType, "ret_trunc");
                        }
                    }
                }
                
                builder.CreateRet(errorCode);
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
                builder.CreateBr(condBB);
            }
            
            // Decision block
            function->insert(function->end(), decisionBB);
            builder.SetInsertPoint(decisionBB);
            builder.CreateStore(builder.getInt1(true), completedFlag);
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
            if (std::holds_alternative<int64_t>(lit->value)) {
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
                // String literal - create global AriaString struct (Phase 4.3)
                const std::string& str = std::get<std::string>(lit->value);
                
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
                    // Get the allocated type from the alloca
                    llvm::Type* loadType = builder.getInt32Ty();  // Default
                    if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(val)) {
                        loadType = allocaInst->getAllocatedType();
                    }
                    
                    // Create the load instruction
                    llvm::Value* loaded = builder.CreateLoad(loadType, val, ident->name);
                    
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
            
            // Get callee name
            if (call->callee->type != ASTNode::NodeType::IDENTIFIER) {
                return nullptr;  // Complex callees not yet supported
            }
            
            IdentifierExpr* callee = static_cast<IdentifierExpr*>(call->callee.get());
            
            // Builtin: print() - outputs a single value to stdout
            if (callee->name == "print") {
                if (call->arguments.size() != 1) {
                    return nullptr;  // print() requires exactly one argument
                }
                
                // Declare printf if not already present
                llvm::Function* printf_func = module->getFunction("printf");
                if (!printf_func) {
                    llvm::FunctionType* printf_type = llvm::FunctionType::get(
                        builder.getInt32Ty(),
                        llvm::PointerType::get(context, 0),
                        true  // vararg
                    );
                    printf_func = llvm::Function::Create(
                        printf_type,
                        llvm::Function::ExternalLinkage,
                        "printf",
                        module.get()
                    );
                }
                
                // Generate argument
                llvm::Value* arg = codegenExpression(call->arguments[0].get());
                if (!arg) {
                    return nullptr;
                }
                
                // Determine format string based on argument type
                llvm::Value* format_str = nullptr;
                std::vector<llvm::Value*> printf_args;
                printf_args.push_back(nullptr);  // Placeholder for format string
                
                llvm::Type* arg_type = arg->getType();
                
                if (arg_type->isPointerTy()) {
                    // String - check if it's AriaString pointer
                    // Try to load as AriaString and extract data field
                    llvm::StructType* aria_string_type = llvm::StructType::getTypeByName(context, "struct.AriaString");
                    if (aria_string_type) {
                        // Load AriaString struct and extract data field
                        llvm::Value* aria_str = builder.CreateLoad(aria_string_type, arg, "aria_str");
                        llvm::Value* str_data = builder.CreateExtractValue(aria_str, 0, "str_data");
                        format_str = builder.CreateGlobalStringPtr("%s\n", "str_fmt");
                        printf_args.push_back(str_data);
                    } else {
                        // Fallback: treat as raw char*
                        format_str = builder.CreateGlobalStringPtr("%s\n", "str_fmt");
                        printf_args.push_back(arg);
                    }
                } else if (arg_type->isIntegerTy()) {
                    // Integer type - determine format based on bit width
                    unsigned bit_width = arg_type->getIntegerBitWidth();
                    
                    if (bit_width < 32) {
                        // Sign-extend to int32 for printf
                        arg = builder.CreateSExt(arg, builder.getInt32Ty());
                        format_str = builder.CreateGlobalStringPtr("%d\n", "int_fmt");
                    } else if (bit_width == 32) {
                        format_str = builder.CreateGlobalStringPtr("%d\n", "int_fmt");
                    } else if (bit_width == 64) {
                        format_str = builder.CreateGlobalStringPtr("%lld\n", "int64_fmt");
                    } else {
                        // For larger integers (128+), print as unsigned 64-bit
                        arg = builder.CreateTrunc(arg, builder.getInt64Ty());
                        format_str = builder.CreateGlobalStringPtr("%llu\n", "int64_fmt");
                    }
                    
                    printf_args.push_back(arg);
                } else if (arg_type->isFloatingPointTy()) {
                    // Float or double
                    if (arg_type->isFloatTy()) {
                        // Promote float to double for printf
                        arg = builder.CreateFPExt(arg, builder.getDoubleTy());
                    }
                    format_str = builder.CreateGlobalStringPtr("%g\n", "float_fmt");
                    printf_args.push_back(arg);
                } else {
                    // Unsupported type
                    return nullptr;
                }
                
                // Set the format string as first argument
                printf_args[0] = format_str;
                
                // Create printf call
                return builder.CreateCall(printf_func, printf_args, "print_call");
            }
            
            // Builtin: stdout_write(string) -> int64
            if (callee->name == "stdout_write") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* arg = codegenExpression(call->arguments[0].get());
                if (!arg || !arg->getType()->isPointerTy()) {
                    return nullptr;
                }
                
                // Declare aria_stdout_write if not already present
                llvm::Function* func = module->getFunction("aria_stdout_write");
                if (!func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        builder.getInt64Ty(),
                        {llvm::PointerType::get(context, 0)},
                        false
                    );
                    func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        "aria_stdout_write",
                        module.get()
                    );
                }
                
                return builder.CreateCall(func, {arg}, "stdout_write_call");
            }
            
            // Builtin: stderr_write(string) -> int64
            if (callee->name == "stderr_write") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* arg = codegenExpression(call->arguments[0].get());
                if (!arg || !arg->getType()->isPointerTy()) {
                    return nullptr;
                }
                
                // Declare aria_stderr_write if not already present
                llvm::Function* func = module->getFunction("aria_stderr_write");
                if (!func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        builder.getInt64Ty(),
                        {llvm::PointerType::get(context, 0)},
                        false
                    );
                    func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        "aria_stderr_write",
                        module.get()
                    );
                }
                
                return builder.CreateCall(func, {arg}, "stderr_write_call");
            }
            
            // Builtin: stddbg_write(string) -> int64
            if (callee->name == "stddbg_write") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* arg = codegenExpression(call->arguments[0].get());
                if (!arg || !arg->getType()->isPointerTy()) {
                    return nullptr;
                }
                
                // Declare aria_stddbg_write if not already present
                llvm::Function* func = module->getFunction("aria_stddbg_write");
                if (!func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        builder.getInt64Ty(),
                        {llvm::PointerType::get(context, 0)},
                        false
                    );
                    func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        "aria_stddbg_write",
                        module.get()
                    );
                }
                
                return builder.CreateCall(func, {arg}, "stddbg_write_call");
            }
            
            // Builtin: stdin_read_line() -> string
            if (callee->name == "stdin_read_line") {
                if (call->arguments.size() != 0) {
                    return nullptr;
                }
                
                // Declare aria_stdin_read_line if not already present
                llvm::Function* func = module->getFunction("aria_stdin_read_line");
                if (!func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0),
                        {},
                        false
                    );
                    func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        "aria_stdin_read_line",
                        module.get()
                    );
                }
                
                return builder.CreateCall(func, {}, "stdin_read_line_call");
            }
            
            // ====================================================================
            // FILE I/O BUILTINS (Phase 4.2)
            // ====================================================================
            
            // Helper lambda to extract char* from AriaString pointer
            auto extractStringData = [&](llvm::Value* aria_str_ptr) -> llvm::Value* {
                // Get AriaString struct type
                llvm::StructType* aria_string_type = llvm::StructType::getTypeByName(context, "struct.AriaString");
                if (!aria_string_type) {
                    std::vector<llvm::Type*> fields = {
                        llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                        llvm::Type::getInt64Ty(context)
                    };
                    aria_string_type = llvm::StructType::create(context, fields, "struct.AriaString");
                }
                
                // Load the AriaString struct from the pointer
                llvm::Value* aria_str = builder.CreateLoad(aria_string_type, aria_str_ptr, "aria_str");
                
                // Extract field 0 (data: char*)
                llvm::Value* data_ptr = builder.CreateExtractValue(aria_str, 0, "str_data");
                
                return data_ptr;
            };
            
            // Builtin: writeFile(path: string, content: string) -> result<int64>
            if (callee->name == "writeFile") {
                if (call->arguments.size() != 2) {
                    return nullptr;
                }
                
                llvm::Value* path_arg = codegenExpression(call->arguments[0].get());
                llvm::Value* content_arg = codegenExpression(call->arguments[1].get());
                if (!path_arg || !content_arg) {
                    return nullptr;
                }
                
                // Extract char* from AriaString struct (path)
                llvm::Value* path_cstr = extractStringData(path_arg);
                
                // Declare aria_write_file_result if not already present
                // Returns AriaResultVoid struct: { void* error, bool is_error }
                llvm::Function* func = module->getFunction("aria_write_file_result");
                if (!func) {
                    llvm::StructType* result_void_type = llvm::StructType::get(
                        llvm::PointerType::get(context, 0),  // error
                        builder.getInt1Ty()                   // is_error
                    );
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        result_void_type,
                        {llvm::PointerType::get(context, 0), llvm::PointerType::get(context, 0)},
                        false
                    );
                    func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        "aria_write_file_result",
                        module.get()
                    );
                }
                
                // content_arg is AriaString*, pass directly
                return builder.CreateCall(func, {path_cstr, content_arg}, "write_file_result");
            }
            
            // Builtin: readFile(path: string) -> result<string>
            if (callee->name == "readFile") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* path_arg = codegenExpression(call->arguments[0].get());
                if (!path_arg) {
                    return nullptr;
                }
                
                // Extract char* from AriaString
                llvm::Value* path_cstr = extractStringData(path_arg);
                
                // Declare aria_read_file_result if not already present
                // Returns AriaResultPtr struct: { void* value, void* error, bool is_error }
                llvm::Function* func = module->getFunction("aria_read_file_result");
                if (!func) {
                    llvm::StructType* result_ptr_type = llvm::StructType::get(
                        llvm::PointerType::get(context, 0),  // value
                        llvm::PointerType::get(context, 0),  // error
                        builder.getInt1Ty()                   // is_error
                    );
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        result_ptr_type,
                        {llvm::PointerType::get(context, 0)},
                        false
                    );
                    func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        "aria_read_file_result",
                        module.get()
                    );
                }
                
                return builder.CreateCall(func, {path_cstr}, "read_file_result");
            }
            
            // Builtin: fileExists(path: string) -> result<bool>
            if (callee->name == "fileExists") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* path_arg = codegenExpression(call->arguments[0].get());
                if (!path_arg) {
                    return nullptr;
                }
                
                // Extract char* from AriaString
                llvm::Value* path_cstr = extractStringData(path_arg);
                
                // Declare aria_file_exists_result if not already present
                // Returns AriaResultBool struct: { bool value, void* error, bool is_error }
                llvm::Function* func = module->getFunction("aria_file_exists_result");
                if (!func) {
                    llvm::StructType* result_bool_type = llvm::StructType::get(
                        builder.getInt1Ty(),                  // value
                        llvm::PointerType::get(context, 0),  // error
                        builder.getInt1Ty()                   // is_error
                    );
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        result_bool_type,
                        {llvm::PointerType::get(context, 0)},
                        false
                    );
                    func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        "aria_file_exists_result",
                        module.get()
                    );
                }
                
                return builder.CreateCall(func, {path_cstr}, "file_exists_result");
            }
            
            // Builtin: fileSize(path: string) -> result<int64>
            if (callee->name == "fileSize") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* path_arg = codegenExpression(call->arguments[0].get());
                if (!path_arg) {
                    return nullptr;
                }
                
                // Extract char* from AriaString
                llvm::Value* path_cstr = extractStringData(path_arg);
                
                // Declare aria_file_size_result if not already present
                // Returns AriaResultI64 struct: { int64 value, void* error, bool is_error }
                llvm::Function* func = module->getFunction("aria_file_size_result");
                if (!func) {
                    llvm::StructType* result_i64_type = llvm::StructType::get(
                        builder.getInt64Ty(),                 // value
                        llvm::PointerType::get(context, 0),  // error
                        builder.getInt1Ty()                   // is_error
                    );
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        result_i64_type,
                        {llvm::PointerType::get(context, 0)},
                        false
                    );
                    func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        "aria_file_size_result",
                        module.get()
                    );
                }
                
                return builder.CreateCall(func, {path_cstr}, "file_size_result");
            }
            
            // Builtin: deleteFile(path: string) -> result<int64>
            if (callee->name == "deleteFile") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* path_arg = codegenExpression(call->arguments[0].get());
                if (!path_arg) {
                    return nullptr;
                }
                
                // Extract char* from AriaString
                llvm::Value* path_cstr = extractStringData(path_arg);
                
                // Declare aria_delete_file_result if not already present
                // Returns AriaResultVoid struct: { void* error, bool is_error }
                llvm::Function* func = module->getFunction("aria_delete_file_result");
                if (!func) {
                    llvm::StructType* result_void_type = llvm::StructType::get(
                        llvm::PointerType::get(context, 0),  // error
                        builder.getInt1Ty()                   // is_error
                    );
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        result_void_type,
                        {llvm::PointerType::get(context, 0)},
                        false
                    );
                    func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        "aria_delete_file_result",
                        module.get()
                    );
                }
                
                return builder.CreateCall(func, {path_cstr}, "delete_file_result");
            }
            
            // Builtin: pathAbsolute(path: string) -> string
            if (callee->name == "pathAbsolute") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* path_arg = codegenExpression(call->arguments[0].get());
                if (!path_arg) {
                    return nullptr;
                }
                
                // Extract char* from AriaString
                llvm::Value* path_cstr = extractStringData(path_arg);
                
                // Declare aria_path_absolute_string if not already present
                llvm::Function* func = module->getFunction("aria_path_absolute_string");
                if (!func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0),
                        {llvm::PointerType::get(context, 0)},
                        false
                    );
                    func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        "aria_path_absolute_string",
                        module.get()
                    );
                }
                
                return builder.CreateCall(func, {path_cstr}, "path_absolute_call");
            }
            
            // Builtin: pathDirname(path: string) -> string
            if (callee->name == "pathDirname") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* path_arg = codegenExpression(call->arguments[0].get());
                if (!path_arg) {
                    return nullptr;
                }
                
                // Extract char* from AriaString
                llvm::Value* path_cstr = extractStringData(path_arg);
                
                // Declare aria_path_dirname_string if not already present
                llvm::Function* func = module->getFunction("aria_path_dirname_string");
                if (!func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0),
                        {llvm::PointerType::get(context, 0)},
                        false
                    );
                    func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        "aria_path_dirname_string",
                        module.get()
                    );
                }
                
                return builder.CreateCall(func, {path_cstr}, "path_dirname_call");
            }
            
            // Builtin: pathBasename(path: string) -> string
            if (callee->name == "pathBasename") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* path_arg = codegenExpression(call->arguments[0].get());
                if (!path_arg) {
                    return nullptr;
                }
                
                // Extract char* from AriaString
                llvm::Value* path_cstr = extractStringData(path_arg);
                
                // Declare aria_path_basename_string if not already present
                llvm::Function* func = module->getFunction("aria_path_basename_string");
                if (!func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0),
                        {llvm::PointerType::get(context, 0)},
                        false
                    );
                    func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        "aria_path_basename_string",
                        module.get()
                    );
                }
                
                return builder.CreateCall(func, {path_cstr}, "path_basename_call");
            }
            
            // Builtin: pathJoin(dir: string, name: string) -> string
            if (callee->name == "pathJoin") {
                if (call->arguments.size() != 2) {
                    return nullptr;
                }
                
                llvm::Value* dir_arg = codegenExpression(call->arguments[0].get());
                llvm::Value* name_arg = codegenExpression(call->arguments[1].get());
                if (!dir_arg || !name_arg) {
                    return nullptr;
                }
                
                // Extract char* from AriaStrings
                llvm::Value* dir_cstr = extractStringData(dir_arg);
                llvm::Value* name_cstr = extractStringData(name_arg);
                
                // Declare aria_path_join_string if not already present
                llvm::Function* func = module->getFunction("aria_path_join_string");
                if (!func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0),
                        {llvm::PointerType::get(context, 0), llvm::PointerType::get(context, 0)},
                        false
                    );
                    func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        "aria_path_join_string",
                        module.get()
                    );
                }
                
                return builder.CreateCall(func, {dir_cstr, name_cstr}, "path_join_call");
            }
            
            // Builtin: pathIsAbsolute(path: string) -> bool
            if (callee->name == "pathIsAbsolute") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* path_arg = codegenExpression(call->arguments[0].get());
                if (!path_arg) {
                    return nullptr;
                }
                
                // Extract char* from AriaString
                llvm::Value* path_cstr = extractStringData(path_arg);
                
                // Declare aria_path_is_absolute if not already present
                llvm::Function* func = module->getFunction("aria_path_is_absolute");
                if (!func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        builder.getInt1Ty(),
                        {llvm::PointerType::get(context, 0)},
                        false
                    );
                    func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        "aria_path_is_absolute",
                        module.get()
                    );
                }
                
                return builder.CreateCall(func, {path_cstr}, "path_is_absolute_call");
            }
            
            // ====================================================================
            // ARENA ALLOCATOR BUILTINS (Phase 4.2.5.2)
            // ====================================================================
            
            if (callee->name == "arena_new") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Function* arena_new_func = module->getFunction("aria_arena_new_handle");
                if (!arena_new_func) {
                    llvm::FunctionType* arena_new_type = llvm::FunctionType::get(
                        builder.getInt64Ty(), {builder.getInt64Ty()}, false);
                    arena_new_func = llvm::Function::Create(arena_new_type,
                        llvm::Function::ExternalLinkage, "aria_arena_new_handle", module.get());
                }
                
                llvm::Value* capacity = codegenExpression(call->arguments[0].get());
                if (!capacity->getType()->isIntegerTy(64)) {
                    capacity = builder.CreateSExtOrTrunc(capacity, builder.getInt64Ty());
                }
                return builder.CreateCall(arena_new_func, {capacity}, "arena_handle");
            }
            
            if (callee->name == "arena_alloc") {
                if (call->arguments.size() != 2) {
                    return nullptr;
                }
                
                llvm::Function* arena_alloc_func = module->getFunction("aria_arena_alloc_handle");
                if (!arena_alloc_func) {
                    llvm::FunctionType* arena_alloc_type = llvm::FunctionType::get(
                        builder.getInt64Ty(), {builder.getInt64Ty(), builder.getInt64Ty()}, false);
                    arena_alloc_func = llvm::Function::Create(arena_alloc_type,
                        llvm::Function::ExternalLinkage, "aria_arena_alloc_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                llvm::Value* size = codegenExpression(call->arguments[1].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                if (!size->getType()->isIntegerTy(64)) {
                    size = builder.CreateSExtOrTrunc(size, builder.getInt64Ty());
                }
                return builder.CreateCall(arena_alloc_func, {handle, size}, "alloc_ptr");
            }
            
            if (callee->name == "arena_reset") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Function* arena_reset_func = module->getFunction("aria_arena_reset_handle");
                if (!arena_reset_func) {
                    llvm::FunctionType* arena_reset_type = llvm::FunctionType::get(
                        builder.getVoidTy(), {builder.getInt64Ty()}, false);
                    arena_reset_func = llvm::Function::Create(arena_reset_type,
                        llvm::Function::ExternalLinkage, "aria_arena_reset_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                builder.CreateCall(arena_reset_func, {handle});
                return builder.getInt32(0);
            }
            
            if (callee->name == "arena_destroy") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Function* arena_destroy_func = module->getFunction("aria_arena_destroy_handle");
                if (!arena_destroy_func) {
                    llvm::FunctionType* arena_destroy_type = llvm::FunctionType::get(
                        builder.getVoidTy(), {builder.getInt64Ty()}, false);
                    arena_destroy_func = llvm::Function::Create(arena_destroy_type,
                        llvm::Function::ExternalLinkage, "aria_arena_destroy_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                builder.CreateCall(arena_destroy_func, {handle});
                return builder.getInt32(0);
            }
            
            if (callee->name == "arena_get_allocated") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Function* func = module->getFunction("aria_arena_get_allocated_handle");
                if (!func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        builder.getInt64Ty(), {builder.getInt64Ty()}, false);
                    func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                        "aria_arena_get_allocated_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                return builder.CreateCall(func, {handle}, "allocated_bytes");
            }
            
            if (callee->name == "arena_get_reserved") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Function* func = module->getFunction("aria_arena_get_reserved_handle");
                if (!func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        builder.getInt64Ty(), {builder.getInt64Ty()}, false);
                    func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                        "aria_arena_get_reserved_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                return builder.CreateCall(func, {handle}, "reserved_bytes");
            }
            
            // ====================================================================
            // POOL ALLOCATOR BUILTINS (Phase 4.2.5.3)
            // ====================================================================
            
            if (callee->name == "pool_new") {
                if (call->arguments.size() != 2) {
                    return nullptr;
                }
                
                llvm::Function* pool_new_func = module->getFunction("aria_pool_new_handle");
                if (!pool_new_func) {
                    llvm::FunctionType* pool_new_type = llvm::FunctionType::get(
                        builder.getInt64Ty(), {builder.getInt64Ty(), builder.getInt64Ty()}, false);
                    pool_new_func = llvm::Function::Create(pool_new_type,
                        llvm::Function::ExternalLinkage, "aria_pool_new_handle", module.get());
                }
                
                llvm::Value* block_size = codegenExpression(call->arguments[0].get());
                llvm::Value* capacity = codegenExpression(call->arguments[1].get());
                if (!block_size->getType()->isIntegerTy(64)) {
                    block_size = builder.CreateSExtOrTrunc(block_size, builder.getInt64Ty());
                }
                if (!capacity->getType()->isIntegerTy(64)) {
                    capacity = builder.CreateSExtOrTrunc(capacity, builder.getInt64Ty());
                }
                return builder.CreateCall(pool_new_func, {block_size, capacity}, "pool_handle");
            }
            
            if (callee->name == "pool_alloc") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Function* pool_alloc_func = module->getFunction("aria_pool_alloc_handle");
                if (!pool_alloc_func) {
                    llvm::FunctionType* pool_alloc_type = llvm::FunctionType::get(
                        builder.getInt64Ty(), {builder.getInt64Ty()}, false);
                    pool_alloc_func = llvm::Function::Create(pool_alloc_type,
                        llvm::Function::ExternalLinkage, "aria_pool_alloc_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                return builder.CreateCall(pool_alloc_func, {handle}, "alloc_ptr");
            }
            
            if (callee->name == "pool_free") {
                if (call->arguments.size() != 2) {
                    return nullptr;
                }
                
                llvm::Function* pool_free_func = module->getFunction("aria_pool_free_handle");
                if (!pool_free_func) {
                    llvm::FunctionType* pool_free_type = llvm::FunctionType::get(
                        builder.getVoidTy(), {builder.getInt64Ty(), builder.getInt64Ty()}, false);
                    pool_free_func = llvm::Function::Create(pool_free_type,
                        llvm::Function::ExternalLinkage, "aria_pool_free_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                llvm::Value* ptr = codegenExpression(call->arguments[1].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                if (!ptr->getType()->isIntegerTy(64)) {
                    ptr = builder.CreateSExtOrTrunc(ptr, builder.getInt64Ty());
                }
                builder.CreateCall(pool_free_func, {handle, ptr});
                return builder.getInt32(0);
            }
            
            if (callee->name == "pool_reset") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Function* pool_reset_func = module->getFunction("aria_pool_reset_handle");
                if (!pool_reset_func) {
                    llvm::FunctionType* pool_reset_type = llvm::FunctionType::get(
                        builder.getVoidTy(), {builder.getInt64Ty()}, false);
                    pool_reset_func = llvm::Function::Create(pool_reset_type,
                        llvm::Function::ExternalLinkage, "aria_pool_reset_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                builder.CreateCall(pool_reset_func, {handle});
                return builder.getInt32(0);
            }
            
            if (callee->name == "pool_destroy") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Function* pool_destroy_func = module->getFunction("aria_pool_destroy_handle");
                if (!pool_destroy_func) {
                    llvm::FunctionType* pool_destroy_type = llvm::FunctionType::get(
                        builder.getVoidTy(), {builder.getInt64Ty()}, false);
                    pool_destroy_func = llvm::Function::Create(pool_destroy_type,
                        llvm::Function::ExternalLinkage, "aria_pool_destroy_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                builder.CreateCall(pool_destroy_func, {handle});
                return builder.getInt32(0);
            }
            
            if (callee->name == "pool_get_total_blocks") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Function* func = module->getFunction("aria_pool_get_total_blocks_handle");
                if (!func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        builder.getInt64Ty(), {builder.getInt64Ty()}, false);
                    func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                        "aria_pool_get_total_blocks_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                return builder.CreateCall(func, {handle}, "total_blocks");
            }
            
            if (callee->name == "pool_get_used_blocks") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Function* func = module->getFunction("aria_pool_get_used_blocks_handle");
                if (!func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        builder.getInt64Ty(), {builder.getInt64Ty()}, false);
                    func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                        "aria_pool_get_used_blocks_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                return builder.CreateCall(func, {handle}, "used_blocks");
            }
            
            // ====================================================================
            // SLAB ALLOCATOR BUILTINS (Phase 4.2.5.4)
            // ====================================================================
            
            if (callee->name == "slab_new") {
                if (call->arguments.size() != 2) {
                    return nullptr;
                }
                
                llvm::Function* slab_new_func = module->getFunction("aria_slab_cache_new_handle");
                if (!slab_new_func) {
                    llvm::FunctionType* slab_new_type = llvm::FunctionType::get(
                        builder.getInt64Ty(), {builder.getInt64Ty(), builder.getInt64Ty()}, false);
                    slab_new_func = llvm::Function::Create(slab_new_type,
                        llvm::Function::ExternalLinkage, "aria_slab_cache_new_handle", module.get());
                }
                
                llvm::Value* object_size = codegenExpression(call->arguments[0].get());
                llvm::Value* slab_size = codegenExpression(call->arguments[1].get());
                if (!object_size->getType()->isIntegerTy(64)) {
                    object_size = builder.CreateSExtOrTrunc(object_size, builder.getInt64Ty());
                }
                if (!slab_size->getType()->isIntegerTy(64)) {
                    slab_size = builder.CreateSExtOrTrunc(slab_size, builder.getInt64Ty());
                }
                return builder.CreateCall(slab_new_func, {object_size, slab_size}, "slab_handle");
            }
            
            if (callee->name == "slab_alloc") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Function* slab_alloc_func = module->getFunction("aria_slab_cache_alloc_handle");
                if (!slab_alloc_func) {
                    llvm::FunctionType* slab_alloc_type = llvm::FunctionType::get(
                        builder.getInt64Ty(), {builder.getInt64Ty()}, false);
                    slab_alloc_func = llvm::Function::Create(slab_alloc_type,
                        llvm::Function::ExternalLinkage, "aria_slab_cache_alloc_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                return builder.CreateCall(slab_alloc_func, {handle}, "alloc_ptr");
            }
            
            if (callee->name == "slab_free") {
                if (call->arguments.size() != 2) {
                    return nullptr;
                }
                
                llvm::Function* slab_free_func = module->getFunction("aria_slab_cache_free_handle");
                if (!slab_free_func) {
                    llvm::FunctionType* slab_free_type = llvm::FunctionType::get(
                        builder.getVoidTy(), {builder.getInt64Ty(), builder.getInt64Ty()}, false);
                    slab_free_func = llvm::Function::Create(slab_free_type,
                        llvm::Function::ExternalLinkage, "aria_slab_cache_free_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                llvm::Value* ptr = codegenExpression(call->arguments[1].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                if (!ptr->getType()->isIntegerTy(64)) {
                    ptr = builder.CreateSExtOrTrunc(ptr, builder.getInt64Ty());
                }
                builder.CreateCall(slab_free_func, {handle, ptr});
                return builder.getInt32(0);
            }
            
            if (callee->name == "slab_destroy") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Function* slab_destroy_func = module->getFunction("aria_slab_cache_destroy_handle");
                if (!slab_destroy_func) {
                    llvm::FunctionType* slab_destroy_type = llvm::FunctionType::get(
                        builder.getVoidTy(), {builder.getInt64Ty()}, false);
                    slab_destroy_func = llvm::Function::Create(slab_destroy_type,
                        llvm::Function::ExternalLinkage, "aria_slab_cache_destroy_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                builder.CreateCall(slab_destroy_func, {handle});
                return builder.getInt32(0);
            }
            
            if (callee->name == "slab_get_total_objects") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Function* func = module->getFunction("aria_slab_cache_get_total_objects_handle");
                if (!func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        builder.getInt64Ty(), {builder.getInt64Ty()}, false);
                    func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                        "aria_slab_cache_get_total_objects_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                return builder.CreateCall(func, {handle}, "total_objects");
            }
            
            if (callee->name == "slab_get_allocated_objects") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Function* func = module->getFunction("aria_slab_cache_get_allocated_objects_handle");
                if (!func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        builder.getInt64Ty(), {builder.getInt64Ty()}, false);
                    func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                        "aria_slab_cache_get_allocated_objects_handle", module.get());
                }
                
                llvm::Value* handle = codegenExpression(call->arguments[0].get());
                if (!handle->getType()->isIntegerTy(64)) {
                    handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
                }
                return builder.CreateCall(func, {handle}, "allocated_objects");
            }
            
            // ====================================================================
            // END ALLOCATOR BUILTINS
            // ====================================================================
            
            // ====================================================================
            // STRING LIBRARY BUILTINS (Phase 4.3) - Delegate to ExprCodegen
            // ====================================================================
            
            // Check if this is a string builtin - delegate to ExprCodegen which has full implementation
            if (callee->name == "string_length" || callee->name == "string_is_empty" ||
                callee->name == "string_equals" || callee->name == "string_concat" ||
                callee->name == "string_substring" || callee->name == "string_contains" ||
                callee->name == "string_starts_with" || callee->name == "string_ends_with" ||
                callee->name == "string_trim" || callee->name == "string_to_upper" ||
                callee->name == "string_to_lower" || callee->name == "string_from_cstr" ||
                callee->name == "string_from_char") {
                
                // Create ExprCodegen instance (same pattern as TEMPLATE_LITERAL case)
                backend::ExprCodegen expr_codegen_inst(context, builder, module.get(), named_values);
                return expr_codegen_inst.codegenCall(call);
            }
            
            // ====================================================================
            // MATH LIBRARY BUILTINS (Phase 4.4)
            // ====================================================================
            
            // abs() - Absolute value
            if (callee->name == "abs") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* arg = codegenExpression(call->arguments[0].get());
                if (!arg) {
                    return nullptr;
                }
                
                // For integers, use select with negation
                if (arg->getType()->isIntegerTy()) {
                    llvm::Value* zero = llvm::ConstantInt::get(arg->getType(), 0);
                    llvm::Value* neg = builder.CreateNeg(arg);
                    llvm::Value* cmp = builder.CreateICmpSLT(arg, zero);
                    return builder.CreateSelect(cmp, neg, arg, "abs");
                }
                // For floats, use llvm.fabs intrinsic
                else if (arg->getType()->isFloatingPointTy()) {
                    llvm::Function* fabs_intrinsic = llvm::Intrinsic::getDeclaration(
                        module.get(), llvm::Intrinsic::fabs, {arg->getType()});
                    return builder.CreateCall(fabs_intrinsic, {arg}, "abs");
                }
                
                return nullptr;
            }
            
            // min() - Minimum of two values
            if (callee->name == "min") {
                if (call->arguments.size() != 2) {
                    return nullptr;
                }
                
                llvm::Value* arg1 = codegenExpression(call->arguments[0].get());
                llvm::Value* arg2 = codegenExpression(call->arguments[1].get());
                if (!arg1 || !arg2) {
                    return nullptr;
                }
                
                // For integers, use ICmp + Select
                if (arg1->getType()->isIntegerTy() && arg2->getType()->isIntegerTy()) {
                    llvm::Value* cmp = builder.CreateICmpSLT(arg1, arg2);
                    return builder.CreateSelect(cmp, arg1, arg2, "min");
                }
                // For floats, use llvm.minnum intrinsic
                else if (arg1->getType()->isFloatingPointTy() && arg2->getType()->isFloatingPointTy()) {
                    llvm::Function* minnum_intrinsic = llvm::Intrinsic::getDeclaration(
                        module.get(), llvm::Intrinsic::minnum, {arg1->getType()});
                    return builder.CreateCall(minnum_intrinsic, {arg1, arg2}, "min");
                }
                
                return nullptr;
            }
            
            // max() - Maximum of two values
            if (callee->name == "max") {
                if (call->arguments.size() != 2) {
                    return nullptr;
                }
                
                llvm::Value* arg1 = codegenExpression(call->arguments[0].get());
                llvm::Value* arg2 = codegenExpression(call->arguments[1].get());
                if (!arg1 || !arg2) {
                    return nullptr;
                }
                
                // For integers, use ICmp + Select
                if (arg1->getType()->isIntegerTy() && arg2->getType()->isIntegerTy()) {
                    llvm::Value* cmp = builder.CreateICmpSGT(arg1, arg2);
                    return builder.CreateSelect(cmp, arg1, arg2, "max");
                }
                // For floats, use llvm.maxnum intrinsic
                else if (arg1->getType()->isFloatingPointTy() && arg2->getType()->isFloatingPointTy()) {
                    llvm::Function* maxnum_intrinsic = llvm::Intrinsic::getDeclaration(
                        module.get(), llvm::Intrinsic::maxnum, {arg1->getType()});
                    return builder.CreateCall(maxnum_intrinsic, {arg1, arg2}, "max");
                }
                
                return nullptr;
            }
            
            // Rounding functions: floor, ceil, round, trunc
            if (callee->name == "floor" || callee->name == "ceil" || 
                callee->name == "round" || callee->name == "trunc") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* arg = codegenExpression(call->arguments[0].get());
                if (!arg) {
                    return nullptr;
                }
                
                // Convert int to double if needed
                if (arg->getType()->isIntegerTy()) {
                    arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
                }
                
                if (!arg->getType()->isFloatingPointTy()) {
                    return nullptr;
                }
                
                llvm::Intrinsic::ID intrinsic_id;
                if (callee->name == "floor") intrinsic_id = llvm::Intrinsic::floor;
                else if (callee->name == "ceil") intrinsic_id = llvm::Intrinsic::ceil;
                else if (callee->name == "round") intrinsic_id = llvm::Intrinsic::round;
                else intrinsic_id = llvm::Intrinsic::trunc;
                
                llvm::Function* intrinsic = llvm::Intrinsic::getDeclaration(
                    module.get(), intrinsic_id, {arg->getType()});
                return builder.CreateCall(intrinsic, {arg}, callee->name);
            }
            
            // Exponential and logarithmic functions
            if (callee->name == "sqrt") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* arg = codegenExpression(call->arguments[0].get());
                if (!arg) {
                    return nullptr;
                }
                
                if (arg->getType()->isIntegerTy()) {
                    arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
                }
                
                if (!arg->getType()->isFloatingPointTy()) {
                    return nullptr;
                }
                
                llvm::Function* intrinsic = llvm::Intrinsic::getDeclaration(
                    module.get(), llvm::Intrinsic::sqrt, {arg->getType()});
                return builder.CreateCall(intrinsic, {arg}, "sqrt");
            }
            
            if (callee->name == "pow") {
                if (call->arguments.size() != 2) {
                    return nullptr;
                }
                
                llvm::Value* base = codegenExpression(call->arguments[0].get());
                llvm::Value* exp = codegenExpression(call->arguments[1].get());
                if (!base || !exp) {
                    return nullptr;
                }
                
                if (base->getType()->isIntegerTy()) {
                    base = builder.CreateSIToFP(base, builder.getDoubleTy());
                }
                if (exp->getType()->isIntegerTy()) {
                    exp = builder.CreateSIToFP(exp, builder.getDoubleTy());
                }
                
                if (!base->getType()->isFloatingPointTy() || !exp->getType()->isFloatingPointTy()) {
                    return nullptr;
                }
                
                llvm::Function* intrinsic = llvm::Intrinsic::getDeclaration(
                    module.get(), llvm::Intrinsic::pow, {base->getType()});
                return builder.CreateCall(intrinsic, {base, exp}, "pow");
            }
            
            if (callee->name == "exp") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* arg = codegenExpression(call->arguments[0].get());
                if (!arg) {
                    return nullptr;
                }
                
                if (arg->getType()->isIntegerTy()) {
                    arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
                }
                
                if (!arg->getType()->isFloatingPointTy()) {
                    return nullptr;
                }
                
                llvm::Function* intrinsic = llvm::Intrinsic::getDeclaration(
                    module.get(), llvm::Intrinsic::exp, {arg->getType()});
                return builder.CreateCall(intrinsic, {arg}, "exp");
            }
            
            if (callee->name == "log") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* arg = codegenExpression(call->arguments[0].get());
                if (!arg) {
                    return nullptr;
                }
                
                if (arg->getType()->isIntegerTy()) {
                    arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
                }
                
                if (!arg->getType()->isFloatingPointTy()) {
                    return nullptr;
                }
                
                llvm::Function* intrinsic = llvm::Intrinsic::getDeclaration(
                    module.get(), llvm::Intrinsic::log, {arg->getType()});
                return builder.CreateCall(intrinsic, {arg}, "log");
            }
            
            if (callee->name == "log10") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* arg = codegenExpression(call->arguments[0].get());
                if (!arg) {
                    return nullptr;
                }
                
                if (arg->getType()->isIntegerTy()) {
                    arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
                }
                
                if (!arg->getType()->isFloatingPointTy()) {
                    return nullptr;
                }
                
                llvm::Function* intrinsic = llvm::Intrinsic::getDeclaration(
                    module.get(), llvm::Intrinsic::log10, {arg->getType()});
                return builder.CreateCall(intrinsic, {arg}, "log10");
            }
            
            if (callee->name == "log2") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* arg = codegenExpression(call->arguments[0].get());
                if (!arg) {
                    return nullptr;
                }
                
                if (arg->getType()->isIntegerTy()) {
                    arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
                }
                
                if (!arg->getType()->isFloatingPointTy()) {
                    return nullptr;
                }
                
                llvm::Function* intrinsic = llvm::Intrinsic::getDeclaration(
                    module.get(), llvm::Intrinsic::log2, {arg->getType()});
                return builder.CreateCall(intrinsic, {arg}, "log2");
            }
            
            // Trigonometric functions
            if (callee->name == "sin") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* arg = codegenExpression(call->arguments[0].get());
                if (!arg) {
                    return nullptr;
                }
                
                if (arg->getType()->isIntegerTy()) {
                    arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
                }
                
                if (!arg->getType()->isFloatingPointTy()) {
                    return nullptr;
                }
                
                llvm::Function* intrinsic = llvm::Intrinsic::getDeclaration(
                    module.get(), llvm::Intrinsic::sin, {arg->getType()});
                return builder.CreateCall(intrinsic, {arg}, "sin");
            }
            
            if (callee->name == "cos") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* arg = codegenExpression(call->arguments[0].get());
                if (!arg) {
                    return nullptr;
                }
                
                if (arg->getType()->isIntegerTy()) {
                    arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
                }
                
                if (!arg->getType()->isFloatingPointTy()) {
                    return nullptr;
                }
                
                llvm::Function* intrinsic = llvm::Intrinsic::getDeclaration(
                    module.get(), llvm::Intrinsic::cos, {arg->getType()});
                return builder.CreateCall(intrinsic, {arg}, "cos");
            }
            
            // Trig functions that need libm (no LLVM intrinsics)
            if (callee->name == "tan" || callee->name == "asin" || callee->name == "acos" || 
                callee->name == "atan") {
                if (call->arguments.size() != 1) {
                    return nullptr;
                }
                
                llvm::Value* arg = codegenExpression(call->arguments[0].get());
                if (!arg) {
                    return nullptr;
                }
                
                if (arg->getType()->isIntegerTy()) {
                    arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
                }
                
                if (!arg->getType()->isFloatingPointTy()) {
                    return nullptr;
                }
                
                // Declare external libm function
                llvm::Function* libm_func = module->getFunction(callee->name);
                if (!libm_func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        builder.getDoubleTy(), {builder.getDoubleTy()}, false);
                    libm_func = llvm::Function::Create(func_type,
                        llvm::Function::ExternalLinkage, callee->name, module.get());
                }
                
                return builder.CreateCall(libm_func, {arg}, callee->name + "_result");
            }
            
            if (callee->name == "atan2") {
                if (call->arguments.size() != 2) {
                    return nullptr;
                }
                
                llvm::Value* y = codegenExpression(call->arguments[0].get());
                llvm::Value* x = codegenExpression(call->arguments[1].get());
                if (!y || !x) {
                    return nullptr;
                }
                
                if (y->getType()->isIntegerTy()) {
                    y = builder.CreateSIToFP(y, builder.getDoubleTy());
                }
                if (x->getType()->isIntegerTy()) {
                    x = builder.CreateSIToFP(x, builder.getDoubleTy());
                }
                
                if (!y->getType()->isFloatingPointTy() || !x->getType()->isFloatingPointTy()) {
                    return nullptr;
                }
                
                // Declare external atan2 from libm
                llvm::Function* atan2_func = module->getFunction("atan2");
                if (!atan2_func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        builder.getDoubleTy(), {builder.getDoubleTy(), builder.getDoubleTy()}, false);
                    atan2_func = llvm::Function::Create(func_type,
                        llvm::Function::ExternalLinkage, "atan2", module.get());
                }
                
                return builder.CreateCall(atan2_func, {y, x}, "atan2_result");
            }
            
            // Math constants
            if (callee->name == "PI") {
                if (call->arguments.size() != 0) {
                    return nullptr;
                }
                return llvm::ConstantFP::get(builder.getDoubleTy(), 3.14159265358979323846);
            }
            
            if (callee->name == "E") {
                if (call->arguments.size() != 0) {
                    return nullptr;
                }
                return llvm::ConstantFP::get(builder.getDoubleTy(), 2.71828182845904523536);
            }
            
            if (callee->name == "TAU") {
                if (call->arguments.size() != 0) {
                    return nullptr;
                }
                return llvm::ConstantFP::get(builder.getDoubleTy(), 6.28318530717958647692);
            }
            
            // Check if this is a specialized generic call
            std::string functionName = callee->name;
            if (!call->specializedMangledName.empty()) {
                // Use the specialized mangled name from type checking
                functionName = call->specializedMangledName;
            }
            
            llvm::Function* calleeFunc = module->getFunction(functionName);
            
            if (!calleeFunc) {
                return nullptr;  // Function not found
            }
            
            // Generate code for arguments
            std::vector<llvm::Value*> args;
            for (const auto& arg : call->arguments) {
                llvm::Value* argVal = codegenExpression(arg.get());
                if (argVal) {
                    args.push_back(argVal);
                }
            }
            
            return builder.CreateCall(calleeFunc, args);
        }
        
        case ASTNode::NodeType::UNARY_OP: {
            // Unary operation
            UnaryExpr* unary = static_cast<UnaryExpr*>(expr);
            
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
                    newVal = builder.CreateAdd(currentVal, one, "inctmp");
                } else {
                    newVal = builder.CreateSub(currentVal, one, "dectmp");
                }
                
                // Store new value
                builder.CreateStore(newVal, var);
                
                // Return new value (prefix behavior - could add postfix support later)
                return newVal;
            }
            
            llvm::Value* operand = codegenExpression(unary->operand.get());
            if (!operand) return nullptr;
            
            switch (unary->op.type) {
                case frontend::TokenType::TOKEN_MINUS:
                    // Negate
                    return builder.CreateNeg(operand, "negtmp");
                
                case frontend::TokenType::TOKEN_BANG:
                    // Logical NOT
                    operand = builder.CreateICmpNE(operand, llvm::ConstantInt::get(operand->getType(), 0), "tobool");
                    return builder.CreateNot(operand, "nottmp");
                
                case frontend::TokenType::TOKEN_TILDE:
                    // Bitwise NOT
                    return builder.CreateNot(operand, "nottmp");
                
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
                
                // Assignment: left must be an lvalue (identifier)
                if (binop->left->type != ASTNode::NodeType::IDENTIFIER) {
                    return nullptr;  // Only simple identifiers supported for now
                }
                
                IdentifierExpr* lhs = static_cast<IdentifierExpr*>(binop->left.get());
                
                // Look up the variable
                auto it = named_values.find(lhs->name);
                if (it == named_values.end()) {
                    return nullptr;  // Variable not found
                }
                
                llvm::Value* var = it->second;
                llvm::Value* result = nullptr;
                
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
                    switch (binop->op.type) {
                        case frontend::TokenType::TOKEN_PLUS_EQUAL:
                            result = builder.CreateAdd(currentVal, rhs, "addtmp");
                            break;
                        case frontend::TokenType::TOKEN_MINUS_EQUAL:
                            result = builder.CreateSub(currentVal, rhs, "subtmp");
                            break;
                        case frontend::TokenType::TOKEN_STAR_EQUAL:
                            result = builder.CreateMul(currentVal, rhs, "multmp");
                            break;
                        case frontend::TokenType::TOKEN_SLASH_EQUAL:
                            result = builder.CreateSDiv(currentVal, rhs, "divtmp");
                            break;
                        case frontend::TokenType::TOKEN_PERCENT_EQUAL:
                            result = builder.CreateSRem(currentVal, rhs, "modtmp");
                            break;
                        default:
                            return nullptr;
                    }
                } else {
                    // Simple assignment
                    result = codegenExpression(binop->right.get());
                    if (!result) return nullptr;
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
                if (name == "trit" || name == "tryte" || name == "nit" || name == "nyte") {
                    isTernary = true;
                    ternaryType = leftType;
                }
            }
            if (!isTernary && rightType && rightType->isPrimitive()) {
                PrimitiveType* prim = static_cast<PrimitiveType*>(rightType);
                const std::string& name = prim->getName();
                if (name == "trit" || name == "tryte" || name == "nit" || name == "nyte") {
                    isTernary = true;
                    ternaryType = rightType;
                }
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
                    return builder.CreateAdd(L, R, "addtmp");
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
                    return builder.CreateSub(L, R, "subtmp");
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
                    return builder.CreateMul(L, R, "multmp");
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
                    return builder.CreateSDiv(L, R, "divtmp");
                case frontend::TokenType::TOKEN_PERCENT:
                    return builder.CreateSRem(L, R, "modtmp");
                
                // Comparison operators (return i1)
                case frontend::TokenType::TOKEN_EQUAL_EQUAL:
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
                    }
                    // Use FCmp for floating point, ICmp for integers
                    if (L->getType()->isFloatingPointTy()) {
                        return builder.CreateFCmpOEQ(L, R, "eqtmp");
                    } else {
                        return builder.CreateICmpEQ(L, R, "eqtmp");
                    }
                case frontend::TokenType::TOKEN_BANG_EQUAL:
                    if (L->getType() != R->getType()) {
                        if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                            if (L->getType()->getIntegerBitWidth() > R->getType()->getIntegerBitWidth()) {
                                R = builder.CreateIntCast(R, L->getType(), true, "cmp_cast");
                            } else {
                                L = builder.CreateIntCast(L, R->getType(), true, "cmp_cast");
                            }
                        }
                    }
                    // Use FCmp for floating point, ICmp for integers
                    if (L->getType()->isFloatingPointTy()) {
                        return builder.CreateFCmpONE(L, R, "netmp");
                    } else {
                        return builder.CreateICmpNE(L, R, "netmp");
                    }
                case frontend::TokenType::TOKEN_LESS:
                    if (L->getType() != R->getType()) {
                        if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                            if (L->getType()->getIntegerBitWidth() > R->getType()->getIntegerBitWidth()) {
                                R = builder.CreateIntCast(R, L->getType(), true, "cmp_cast");
                            } else {
                                L = builder.CreateIntCast(L, R->getType(), true, "cmp_cast");
                            }
                        }
                    }
                    // Use FCmp for floating point, ICmp for integers
                    if (L->getType()->isFloatingPointTy()) {
                        return builder.CreateFCmpOLT(L, R, "lttmp");
                    } else {
                        return builder.CreateICmpSLT(L, R, "lttmp");
                    }
                case frontend::TokenType::TOKEN_LESS_EQUAL:
                    if (L->getType() != R->getType()) {
                        if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                            if (L->getType()->getIntegerBitWidth() > R->getType()->getIntegerBitWidth()) {
                                R = builder.CreateIntCast(R, L->getType(), true, "cmp_cast");
                            } else {
                                L = builder.CreateIntCast(L, R->getType(), true, "cmp_cast");
                            }
                        }
                    }
                    // Use FCmp for floating point, ICmp for integers
                    if (L->getType()->isFloatingPointTy()) {
                        return builder.CreateFCmpOLE(L, R, "letmp");
                    } else {
                        return builder.CreateICmpSLE(L, R, "letmp");
                    }
                case frontend::TokenType::TOKEN_GREATER:
                    if (L->getType() != R->getType()) {
                        if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                            if (L->getType()->getIntegerBitWidth() > R->getType()->getIntegerBitWidth()) {
                                R = builder.CreateIntCast(R, L->getType(), true, "cmp_cast");
                            } else {
                                L = builder.CreateIntCast(L, R->getType(), true, "cmp_cast");
                            }
                        }
                    }
                    // Use FCmp for floating point, ICmp for integers
                    if (L->getType()->isFloatingPointTy()) {
                        return builder.CreateFCmpOGT(L, R, "gttmp");
                    } else {
                        return builder.CreateICmpSGT(L, R, "gttmp");
                    }
                case frontend::TokenType::TOKEN_GREATER_EQUAL:
                    if (L->getType() != R->getType()) {
                        if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                            if (L->getType()->getIntegerBitWidth() > R->getType()->getIntegerBitWidth()) {
                                R = builder.CreateIntCast(R, L->getType(), true, "cmp_cast");
                            } else {
                                L = builder.CreateIntCast(L, R->getType(), true, "cmp_cast");
                            }
                        }
                    }
                    // Use FCmp for floating point, ICmp for integers
                    if (L->getType()->isFloatingPointTy()) {
                        return builder.CreateFCmpOGE(L, R, "getmp");
                    } else {
                        return builder.CreateICmpSGE(L, R, "getmp");
                    }
                
                // SPACESHIP OPERATOR (<=>)
                // Three-way comparison: returns -1 if left < right, 0 if equal, 1 if left > right
                case frontend::TokenType::TOKEN_SPACESHIP: {
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
                    return builder.CreateAnd(L, R, "andtmp");
                case frontend::TokenType::TOKEN_PIPE:
                    return builder.CreateOr(L, R, "ortmp");
                case frontend::TokenType::TOKEN_CARET:
                    return builder.CreateXor(L, R, "xortmp");
                case frontend::TokenType::TOKEN_SHIFT_LEFT:
                    return builder.CreateShl(L, R, "shltmp");
                case frontend::TokenType::TOKEN_SHIFT_RIGHT:
                    return builder.CreateAShr(L, R, "ashrtmp");
                
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
            // Unwrap expression: result ? default_value
            // TODO: When full result types are implemented, check for error and use default if needed
            // For now, simplified implementation: just return the result value
            UnwrapExpr* unwrap = static_cast<UnwrapExpr*>(expr);
            
            // Generate code for the result expression
            llvm::Value* resultVal = codegenExpression(unwrap->result.get());
            if (!resultVal) return nullptr;
            
            // TODO: Full implementation would:
            // 1. Check if result.err is NULL
            // 2. If NULL, return result.val
            // 3. If not NULL, return default_value
            // For now, just return the result value directly
            return resultVal;
        }
        
        case ASTNode::NodeType::OBJECT_LITERAL: {
            // Object literal - struct instantiation: Point{ x: 10, y: 20 }
            ObjectLiteralExpr* objLit = static_cast<ObjectLiteralExpr*>(expr);
            
            // Get the struct type from the type name
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
                
                // Store the value
                builder.CreateStore(field_value, field_ptr);
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
        
        case ASTNode::NodeType::MEMBER_ACCESS: {
            // Member access - enum variants, struct fields, vector components
            MemberAccessExpr* member = static_cast<MemberAccessExpr*>(expr);
            
            // Check if this is an enum variant access (EnumName.VARIANT)
            if (member->object->type == ASTNode::NodeType::IDENTIFIER) {
                IdentifierExpr* ident = static_cast<IdentifierExpr*>(member->object.get());
                std::string fullName = ident->name + "." + member->member;
                
                // Check if this is an enum variant (registered in enum_constants map)
                auto enum_it = enum_constants.find(fullName);
                if (enum_it != enum_constants.end()) {
                    // Return the constant integer value for this enum variant
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
                }
            } else {
                // For complex expressions, generate code normally
                object_ptr = codegenExpression(member->object.get());
            }
            
            if (!object_ptr) {
                return nullptr;
            }
            
            // Look up the Aria type of this value
            auto type_it = value_types.find(object_ptr);
            if (type_it == value_types.end()) {
                // No type information available
                return nullptr;
            }
            
            Type* aria_type = type_it->second;
            
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
            
            // Handle struct member access
            if (aria_type->getKind() != TypeKind::STRUCT) {
                // Not a struct type
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
                // For complex expressions, generate code normally
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
            if (auto* ptr_type = llvm::dyn_cast<llvm::PointerType>(array_ptr->getType())) {
                // Modern LLVM uses opaque pointers, need to track element type separately
                // For now, assume int64 (common case for literals)
                elem_type = llvm::Type::getInt64Ty(context);
            } else if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(array_ptr)) {
                elem_type = alloca->getAllocatedType();
            } else {
                // Fallback to int32
                elem_type = llvm::Type::getInt32Ty(context);
            }
            
            // Create GEP to access element at index
            llvm::Value* elem_ptr = builder.CreateGEP(elem_type, array_ptr, index_value, "arrayidx");
            
            // Load and return the element value
            return builder.CreateLoad(elem_type, elem_ptr, "elem");
        }
        
        case ASTNode::NodeType::TEMPLATE_LITERAL: {
            // Template literal with interpolation: `Hello &{name}!`
            // Delegate to ExprCodegen::codegenTemplateLiteral for detailed implementation
            TemplateLiteralExpr* templateLit = static_cast<TemplateLiteralExpr*>(expr);
            
            // Create ExprCodegen instance
            backend::ExprCodegen expr_codegen(context, builder, module.get(), named_values);
            
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
            // Phase 4.6: Await expression - suspend coroutine until operand completes
            AwaitExpr* awaitExpr = static_cast<AwaitExpr*>(expr);
            
            // Step 1: Evaluate the operand (should be an async function call returning coroutine handle)
            llvm::Value* future_value = codegenExpression(awaitExpr->operand.get());
            if (!future_value) return nullptr;
            
            // Get coroutine intrinsics
            llvm::Function* coroResumeFunc = llvm::Intrinsic::getDeclaration(
                module.get(),
                llvm::Intrinsic::coro_resume
            );
            
            llvm::Function* coroSaveFunc = llvm::Intrinsic::getDeclaration(
                module.get(),
                llvm::Intrinsic::coro_save
            );
            
            llvm::Function* coroSuspendFunc = llvm::Intrinsic::getDeclaration(
                module.get(),
                llvm::Intrinsic::coro_suspend
            );
            
            // Step 2: If the operand is a coroutine handle (ptr), resume it
            if (future_value->getType()->isPointerTy()) {
                builder.CreateCall(coroResumeFunc, {future_value});
            }
            
            // Get current function (we're inside an async function)
            llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();
            
            // Use the awaited coroutine handle as the current handle for now
            // TODO: Proper handle management when we have full Future<T> system
            llvm::Value* current_handle = future_value;
            
            // Step 3: Save state and suspend
            llvm::Value* save_token = builder.CreateCall(coroSaveFunc, {current_handle}, "await.save");
            
            // Suspend (not final - this is an intermediate suspension point)
            llvm::Value* is_final = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
            llvm::Value* suspend_result = builder.CreateCall(
                coroSuspendFunc,
                {save_token, is_final},
                "await.suspend"
            );
            
            // Step 4: Create resume and cleanup blocks
            llvm::BasicBlock* resume_block = llvm::BasicBlock::Create(
                context,
                "await.resume",
                currentFunc
            );
            
            llvm::BasicBlock* cleanup_block = llvm::BasicBlock::Create(
                context,
                "await.cleanup",
                currentFunc
            );
            
            // Step 5: Switch on suspend result
            llvm::SwitchInst* suspend_switch = builder.CreateSwitch(suspend_result, resume_block, 1);
            suspend_switch->addCase(
                llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 1),
                cleanup_block
            );
            
            // Step 6: Generate cleanup block
            builder.SetInsertPoint(cleanup_block);
            builder.CreateUnreachable();  // LLVM's coroutine pass will handle this
            
            // Step 7: Generate resume block - continue execution here
            builder.SetInsertPoint(resume_block);
            
            // For now, return the future value itself
            // TODO: Extract actual result from Future<T> when trait system is ready
            return future_value;
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
    
    // Get the current scope's defer blocks
    std::vector<BlockStmt*>& current_scope_defers = defer_stack.back();
    
    // Execute in LIFO order (reverse)
    for (auto it = current_scope_defers.rbegin(); it != current_scope_defers.rend(); ++it) {
        BlockStmt* defer_block = *it;
        for (const auto& statement : defer_block->statements) {
            codegenStatement(statement.get());
        }
    }
}

void aria::IRGenerator::executeFunctionDefers() {
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
}
