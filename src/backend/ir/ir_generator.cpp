#include "backend/ir/ir_generator.h"
#include "frontend/ast/ast_node.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/expr.h"
#include "frontend/token.h"
#include "frontend/sema/type.h"  // Full type definitions needed
#include "frontend/sema/generic_resolver.h"  // For Specialization
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

// Helper function to map type name strings to LLVM types
llvm::Type* IRGenerator::mapTypeFromName(const std::string& type_name) {
    // Check for void
    if (type_name == "void") {
        return builder.getVoidTy();
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
    
    // String type - represented as i8* (pointer to null-terminated char array)
    if (type_name == "string") {
        return llvm::PointerType::get(context, 0);
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
        
        // Generate code for each declaration
        for (const auto& decl : program->declarations) {
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
                    param_types.push_back(mapTypeFromName(pnode->typeName));
                }
                
                llvm::Type* return_type = mapTypeFromName(funcDecl->returnType);
                llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_types, false);
                
                llvm::Function* func = llvm::Function::Create(
                    func_type,
                    llvm::Function::ExternalLinkage,
                    funcDecl->funcName,
                    module.get()
                );
                
                // Skip body generation if no body (extern declaration)
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
                        named_values[param->paramName] = &arg;
                    }
                    idx++;
                }
                
                // Generate function body
                llvm::Value* bodyVal = codegenStatement(funcDecl->body.get());
                (void)bodyVal;  // Suppress unused warning
                
                // Add terminator if missing
                if (!builder.GetInsertBlock()->getTerminator()) {
                    if (return_type->isVoidTy()) {
                        builder.CreateRetVoid();
                    } else {
                        builder.CreateRet(llvm::ConstantInt::get(return_type, 0));
                    }
                }
                
                // Clean up symbol table
                for (const auto& param : funcDecl->parameters) {
                    ParameterNode* pnode = static_cast<ParameterNode*>(param.get());
                    named_values.erase(pnode->paramName);
                }
            }
        }
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
        llvm::Type* returnType = mapTypeFromName(funcDecl->returnType);
        
        // Build parameter types with proper type mapping
        std::vector<llvm::Type*> paramTypes;
        for (const auto& param : funcDecl->parameters) {
            ParameterNode* pnode = static_cast<ParameterNode*>(param.get());
            paramTypes.push_back(mapTypeFromName(pnode->typeName));
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
            
            // Allocate stack space for the variable with proper type mapping
            llvm::Type* varType = mapTypeFromName(varDecl->typeName);
            llvm::AllocaInst* alloca = builder.CreateAlloca(varType, nullptr, varDecl->varName);
            
            // Generate initializer if present
            if (varDecl->initializer) {
                llvm::Value* initVal = codegenExpression(varDecl->initializer.get());
                if (initVal) {
                    builder.CreateStore(initVal, alloca);
                }
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
            // For loop: for (init; cond; update) { body }
            ForStmt* forStmt = static_cast<ForStmt*>(stmt);
            
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
            
            // Handle different literal types
            if (std::holds_alternative<int64_t>(lit->value)) {
                int64_t val = std::get<int64_t>(lit->value);
                // Default to int32 for integer literals (most common case)
                // TODO: Infer type from context (return type, parameter type, etc.)
                return llvm::ConstantInt::get(builder.getInt32Ty(), val);
            } else if (std::holds_alternative<double>(lit->value)) {
                double val = std::get<double>(lit->value);
                return llvm::ConstantFP::get(builder.getDoubleTy(), val);
            } else if (std::holds_alternative<bool>(lit->value)) {
                bool val = std::get<bool>(lit->value);
                return builder.getInt1(val);
            } else if (std::holds_alternative<std::string>(lit->value)) {
                // String literal - create global constant string
                std::string val = std::get<std::string>(lit->value);
                return builder.CreateGlobalStringPtr(val, "str");
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
                    return builder.CreateLoad(builder.getInt32Ty(), val, ident->name);
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
            llvm::Function* calleeFunc = module->getFunction(callee->name);
            
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
            
            // For all other operators, evaluate both operands
            llvm::Value* L = codegenExpression(binop->left.get());
            llvm::Value* R = codegenExpression(binop->right.get());
            
            if (!L || !R) {
                return nullptr;
            }
            
            // Generate appropriate operation based on operator
            switch (binop->op.type) {
                // Arithmetic operators
                case frontend::TokenType::TOKEN_PLUS:
                    return builder.CreateAdd(L, R, "addtmp");
                case frontend::TokenType::TOKEN_MINUS:
                    return builder.CreateSub(L, R, "subtmp");
                case frontend::TokenType::TOKEN_STAR:
                    return builder.CreateMul(L, R, "multmp");
                case frontend::TokenType::TOKEN_SLASH:
                    return builder.CreateSDiv(L, R, "divtmp");
                case frontend::TokenType::TOKEN_PERCENT:
                    return builder.CreateSRem(L, R, "modtmp");
                
                // Comparison operators (return i1)
                case frontend::TokenType::TOKEN_EQUAL_EQUAL:
                    return builder.CreateICmpEQ(L, R, "eqtmp");
                case frontend::TokenType::TOKEN_BANG_EQUAL:
                    return builder.CreateICmpNE(L, R, "netmp");
                case frontend::TokenType::TOKEN_LESS:
                    return builder.CreateICmpSLT(L, R, "lttmp");
                case frontend::TokenType::TOKEN_LESS_EQUAL:
                    return builder.CreateICmpSLE(L, R, "letmp");
                case frontend::TokenType::TOKEN_GREATER:
                    return builder.CreateICmpSGT(L, R, "gttmp");
                case frontend::TokenType::TOKEN_GREATER_EQUAL:
                    return builder.CreateICmpSGE(L, R, "getmp");
                
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
