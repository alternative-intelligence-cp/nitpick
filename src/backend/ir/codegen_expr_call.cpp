// codegen_expr_call.cpp — Split from codegen_expr.cpp for parallel builds (v0.8.2)
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
 * Generate code for function calls
 */
llvm::Value* ExprCodegen::codegenCall(CallExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null call expression");
    }
    
    // Handle module member function calls (e.g., math_utils.add(...))
    // Also handles namespace-qualified static method calls (e.g., string.from_char(...))
    // And UFCS instance method calls (e.g., s.length() -> string_length(s))
    MemberAccessExpr* member_access = dynamic_cast<MemberAccessExpr*>(expr->callee.get());
    if (member_access) {
        // The object should be an identifier (the module/type name or variable)
        IdentifierExpr* ident = dynamic_cast<IdentifierExpr*>(member_access->object.get());
        if (!ident) {
            throw std::runtime_error("Member access must have identifier as base");
        }
        
        // GPU/PTX Backend - Phase 4: GPU Intrinsics
        // Intercept gpu.* calls and map to LLVM NVVM intrinsics
        if (ident->name == "gpu") {
            return codegenGPUIntrinsic(member_access->member, expr);
        }

        std::string mangled_func_name;
        std::vector<ASTNodePtr> call_args = expr->arguments;

        // FIRST: Check if this is a MODULE namespace (e.g., helper.add() where helper is a loaded module)
        // Module functions are called directly by name, not with type prefixes
        // This check must come before UFCS/static method checks
        // Note: We detect modules by checking if there's a function with the exact member name
        // because IR generation doesn't have access to the symbol table's MODULE symbols
        llvm::Function* direct_func = module->getFunction(member_access->member);
        if (direct_func) {
            // This is likely a module function call - use the member name directly
            mangled_func_name = member_access->member;
            // Don't modify arguments - module functions take their arguments normally
        } else {
            // Check if this is a variable (UFCS) or a type name (static method)
            // If it's in named_values, it's a variable
            auto var_it = named_values.find(ident->name);
            if (var_it != named_values.end()) {
            // UFCS: variable.method() -> TypeName_method(variable)
            // Get the LLVM type and map it to a type name
            llvm::Value* var_value = var_it->second;
            llvm::Type* var_type = var_value->getType();

            // Determine the type name from Aria type tracking or LLVM type
            std::string type_name;

            // First, check if we have the Aria type name tracked
            auto type_it = var_aria_types.find(ident->name);
            if (type_it != var_aria_types.end()) {
                // Use the exact Aria type name (e.g., "array", "string", "MyStruct")
                type_name = type_it->second;
            } else {
                // Fallback to LLVM type heuristics for older code paths
                if (var_type->isPointerTy()) {
                    // Pointer types are usually string, array, or user structs
                    // Default to "string" for pointer types without tracked Aria type
                    type_name = "string";
                } else if (var_type->isIntegerTy(64)) {
                    type_name = "int64";
                } else if (var_type->isIntegerTy(32)) {
                    type_name = "int32";
                } else if (var_type->isIntegerTy(16)) {
                    type_name = "int16";
                } else if (var_type->isIntegerTy(8)) {
                    type_name = "int8";
                } else if (var_type->isIntegerTy(1)) {
                    type_name = "bool";
                } else if (var_type->isDoubleTy()) {
                    type_name = "flt64";
                } else if (var_type->isFloatTy()) {
                    type_name = "flt32";
                } else {
                    // Fallback: use identifier name (old behavior - won't work for UFCS)
                    type_name = ident->name;
                }
            }

            // ====================================================================
            // ATOMIC<T> METHOD DISPATCH
            // ====================================================================
            // Handle atomic<T> method calls by dispatching to runtime C functions
            // counter.load() → aria_atomic_TYPE_load(&counter, SEQCST)
            // Supports both "atomic<int32>" and "atomic_int32" type name formats
            if (type_name.find("atomic<") == 0 || type_name.find("atomic_") == 0) {
                // Extract the inner type from atomic<TYPE> or atomic_TYPE
                std::string inner_type;
                if (type_name.find("atomic<") == 0) {
                    size_t start = type_name.find('<') + 1;
                    size_t end = type_name.find('>');
                    if (start == std::string::npos || end == std::string::npos) {
                        throw std::runtime_error("Malformed atomic type: " + type_name);
                    }
                    inner_type = type_name.substr(start, end - start);
                } else {
                    // atomic_int32 → int32
                    inner_type = type_name.substr(7);  // Skip "atomic_"
                }
                
                // Map Aria type to runtime type name
                std::string runtime_type = inner_type;  // int32 → int32, tbb32 → tbb32, etc.
                
                // Get the atomic variable (already a pointer from atomic_new)
                llvm::Value* atomic_ptr = var_value;
                
                // Dispatch based on method name
                std::string method_name = member_access->member;
                
                if (method_name == "load") {
                    // atomic.load() → aria_atomic_TYPE_load(atomic_ptr, SEQCST)
                    std::string func_name = "aria_atomic_" + runtime_type + "_load";
                    llvm::Function* load_func = module->getFunction(func_name);
                    
                    if (!load_func) {
                        // Determine return type based on inner type
                        llvm::Type* ret_type = nullptr;
                        if (inner_type == "int8") ret_type = builder.getInt8Ty();
                        else if (inner_type == "int16") ret_type = builder.getInt16Ty();
                        else if (inner_type == "int32") ret_type = builder.getInt32Ty();
                        else if (inner_type == "int64") ret_type = builder.getInt64Ty();
                        else if (inner_type == "bool") ret_type = builder.getInt1Ty();
                        else throw std::runtime_error("Unsupported atomic type: " + inner_type);
                        
                        // Signature: TYPE aria_atomic_TYPE_load(AriaAtomicTYPE*, int32)
                        llvm::FunctionType* func_type = llvm::FunctionType::get(
                            ret_type,
                            {llvm::PointerType::get(context, 0),  // atomic pointer
                             builder.getInt32Ty()},               // memory order
                            false
                        );
                        
                        load_func = llvm::Function::Create(
                            func_type,
                            llvm::Function::ExternalLinkage,
                            func_name,
                            module
                        );
                    }
                    
                    // Memory order: ARIA_MEMORY_ORDER_SEQ_CST = 4
                    llvm::Value* order = llvm::ConstantInt::get(builder.getInt32Ty(), 4);
                    return builder.CreateCall(load_func, {atomic_ptr, order}, "atomic_load");
                }
                else if (method_name == "store") {
                    // atomic.store(value) → aria_atomic_TYPE_store(atomic_ptr, value, SEQCST)
                    if (expr->arguments.size() != 1) {
                        throw std::runtime_error("atomic.store() requires exactly one argument");
                    }
                    
                    std::string func_name = "aria_atomic_" + runtime_type + "_store";
                    llvm::Function* store_func = module->getFunction(func_name);
                    
                    llvm::Value* value_arg = codegenExpressionNode(expr->arguments[0].get(), this);
                    
                    if (!store_func) {
                        // Signature: void aria_atomic_TYPE_store(AriaAtomicTYPE*, TYPE, int32)
                        llvm::FunctionType* func_type = llvm::FunctionType::get(
                            builder.getVoidTy(),
                            {llvm::PointerType::get(context, 0),  // atomic pointer
                             value_arg->getType(),                // value type
                             builder.getInt32Ty()},               // memory order
                            false
                        );
                        
                        store_func = llvm::Function::Create(
                            func_type,
                            llvm::Function::ExternalLinkage,
                            func_name,
                            module
                        );
                    }
                    
                    llvm::Value* order = llvm::ConstantInt::get(builder.getInt32Ty(), 4);
                    builder.CreateCall(store_func, {atomic_ptr, value_arg, order});
                    return nullptr;
                }
                else if (method_name == "swap") {
                    // atomic.swap(value) → aria_atomic_TYPE_exchange(atomic_ptr, value, SEQCST)
                    if (expr->arguments.size() != 1) {
                        throw std::runtime_error("atomic.swap() requires exactly one argument");
                    }
                    
                    std::string func_name = "aria_atomic_" + runtime_type + "_exchange";
                    llvm::Value* value_arg = codegenExpressionNode(expr->arguments[0].get(), this);
                    llvm::Function* swap_func = module->getFunction(func_name);
                    
                    if (!swap_func) {
                        // Signature: TYPE aria_atomic_TYPE_exchange(AriaAtomicTYPE*, TYPE, int32)
                        llvm::FunctionType* func_type = llvm::FunctionType::get(
                            value_arg->getType(),                // returns old value
                            {llvm::PointerType::get(context, 0), // atomic pointer
                             value_arg->getType(),               // new value
                             builder.getInt32Ty()},              // memory order
                            false
                        );
                        
                        swap_func = llvm::Function::Create(
                            func_type,
                            llvm::Function::ExternalLinkage,
                            func_name,
                            module
                        );
                    }
                    
                    llvm::Value* order = llvm::ConstantInt::get(builder.getInt32Ty(), 4);
                    return builder.CreateCall(swap_func, {atomic_ptr, value_arg, order}, "atomic_swap");
                }
                else if (method_name == "fetch_add") {
                    // atomic.fetch_add(delta) → aria_atomic_TYPE_fetch_add(atomic_ptr, delta, SEQCST)
                    if (expr->arguments.size() != 1) {
                        throw std::runtime_error("atomic.fetch_add() requires exactly one argument");
                    }
                    
                    std::string func_name = "aria_atomic_" + runtime_type + "_fetch_add";
                    llvm::Value* delta_arg = codegenExpressionNode(expr->arguments[0].get(), this);
                    llvm::Function* add_func = module->getFunction(func_name);
                    
                    if (!add_func) {
                        llvm::FunctionType* func_type = llvm::FunctionType::get(
                            delta_arg->getType(),
                            {llvm::PointerType::get(context, 0),
                             delta_arg->getType(),
                             builder.getInt32Ty()},
                            false
                        );
                        
                        add_func = llvm::Function::Create(
                            func_type,
                            llvm::Function::ExternalLinkage,
                            func_name,
                            module
                        );
                    }
                    
                    llvm::Value* order = llvm::ConstantInt::get(builder.getInt32Ty(), 4);
                    return builder.CreateCall(add_func, {atomic_ptr, delta_arg, order}, "atomic_fetch_add");
                }
                else if (method_name == "fetch_sub") {
                    // atomic.fetch_sub(delta) → aria_atomic_TYPE_fetch_sub(atomic_ptr, delta, SEQCST)
                    if (expr->arguments.size() != 1) {
                        throw std::runtime_error("atomic.fetch_sub() requires exactly one argument");
                    }
                    
                    std::string func_name = "aria_atomic_" + runtime_type + "_fetch_sub";
                    llvm::Value* delta_arg = codegenExpressionNode(expr->arguments[0].get(), this);
                    llvm::Function* sub_func = module->getFunction(func_name);
                    
                    if (!sub_func) {
                        llvm::FunctionType* func_type = llvm::FunctionType::get(
                            delta_arg->getType(),
                            {llvm::PointerType::get(context, 0),
                             delta_arg->getType(),
                             builder.getInt32Ty()},
                            false
                        );
                        
                        sub_func = llvm::Function::Create(
                            func_type,
                            llvm::Function::ExternalLinkage,
                            func_name,
                            module
                        );
                    }
                    
                    llvm::Value* order = llvm::ConstantInt::get(builder.getInt32Ty(), 4);
                    return builder.CreateCall(sub_func, {atomic_ptr, delta_arg, order}, "atomic_fetch_sub");
                }
                else {
                    throw std::runtime_error("Unknown atomic method: " + method_name);
                }
            }

            // ====================================================================
            // ANY TYPE METHOD DISPATCH
            // ====================================================================
            // Handle any type method calls:
            //   box.get::<int64>() → aria_any_get(any_ptr, type_id, sizeof_T)
            //   box.set::<int64>(val) → aria_any_set(any_ptr, &val, type_id, sizeof_T)
            //   box.resolve::<int64>() → aria_any_resolve(any_ptr, type_id, sizeof_T) → T*
            if (type_name == "any" || type_name == "wild any" || type_name == "wildx any") {
                std::string method_name = member_access->member;
                llvm::Value* any_ptr = var_value;

                // Load the any struct {ptr, i64} from the alloca
                llvm::Type* any_struct_type = llvm::StructType::get(context, {
                    llvm::PointerType::get(context, 0),  // data pointer
                    builder.getInt64Ty()                  // size
                });

                if (method_name == "get") {
                    // any.get::<T>() — Extract the data pointer and bitcast/load as T
                    if (expr->explicitTypeArgs.empty()) {
                        throw std::runtime_error("any.get() requires a type argument via turbofish: .get::<T>()");
                    }
                    std::string target_type_name = expr->explicitTypeArgs[0];

                    // Determine target LLVM type
                    llvm::Type* target_llvm_type = nullptr;
                    if (target_type_name == "int8") target_llvm_type = builder.getInt8Ty();
                    else if (target_type_name == "int16") target_llvm_type = builder.getInt16Ty();
                    else if (target_type_name == "int32") target_llvm_type = builder.getInt32Ty();
                    else if (target_type_name == "int64") target_llvm_type = builder.getInt64Ty();
                    else if (target_type_name == "bool") target_llvm_type = builder.getInt1Ty();
                    else if (target_type_name == "flt32") target_llvm_type = builder.getFloatTy();
                    else if (target_type_name == "flt64") target_llvm_type = builder.getDoubleTy();
                    else target_llvm_type = llvm::PointerType::get(context, 0);  // Structs/pointers

                    // Load the any struct from the alloca
                    llvm::Value* any_val = builder.CreateLoad(any_struct_type, any_ptr, "any_load");
                    // Extract the data pointer (field 0)
                    llvm::Value* data_ptr = builder.CreateExtractValue(any_val, 0, "any_data");
                    // Load the value through the data pointer as target type
                    return builder.CreateLoad(target_llvm_type, data_ptr, "any_get");
                }
                else if (method_name == "set") {
                    // any.set::<T>(value) — Store value through the data pointer
                    if (expr->explicitTypeArgs.empty()) {
                        throw std::runtime_error("any.set() requires a type argument via turbofish: .set::<T>(val)");
                    }
                    if (expr->arguments.size() != 1) {
                        throw std::runtime_error("any.set::<T>() requires exactly one argument");
                    }

                    // Generate the value to store
                    llvm::Value* store_val = codegenExpressionNode(expr->arguments[0].get(), this);

                    // Load the any struct from the alloca
                    llvm::Value* any_val = builder.CreateLoad(any_struct_type, any_ptr, "any_load");
                    // Extract the data pointer (field 0)
                    llvm::Value* data_ptr = builder.CreateExtractValue(any_val, 0, "any_data");
                    // Store the value through the data pointer
                    builder.CreateStore(store_val, data_ptr);
                    // Return void-like (store has no meaningful return)
                    return llvm::ConstantInt::get(builder.getInt32Ty(), 0);
                }
                else if (method_name == "resolve") {
                    // any.resolve::<T>() — Extract the data pointer (consuming)
                    // Returns T-> (a fat pointer to the underlying data)
                    if (expr->explicitTypeArgs.empty()) {
                        throw std::runtime_error("any.resolve() requires a type argument via turbofish: .resolve::<T>()");
                    }

                    // Load the any struct from the alloca
                    llvm::Value* any_val = builder.CreateLoad(any_struct_type, any_ptr, "any_load");
                    // Extract the data pointer (field 0) — this IS the resolved pointer
                    llvm::Value* data_ptr = builder.CreateExtractValue(any_val, 0, "any_resolve");
                    return data_ptr;
                }
                else {
                    throw std::runtime_error("Unknown method '" + method_name + "' on any type. Use .get::<T>(), .set::<T>(val), or .resolve::<T>()");
                }
            }

            // ====================================================================
            // v0.2.36: DYN TRAIT DYNAMIC DISPATCH
            // ====================================================================
            // When calling a method on a dyn Trait variable, use vtable dispatch:
            //   item.describe() where item: dyn Describable
            //   → extract data_ptr from fat_ptr[0]
            //   → extract vtable_ptr from fat_ptr[1]
            //   → load method fn ptr from vtable at correct offset
            //   → indirect call with data_ptr as first arg
            if (type_name.substr(0, 4) == "dyn ") {
                std::string traitName = type_name.substr(4);
                std::string methodName = member_access->member;

                // Load the fat pointer struct { ptr data, ptr vtable } from the alloca
                llvm::Type* ptrTy = llvm::PointerType::get(context, 0);
                llvm::StructType* fatPtrTy = llvm::StructType::get(context, {ptrTy, ptrTy});
                llvm::Value* fatPtr = builder.CreateLoad(fatPtrTy, var_value, "dyn_fat");

                // Extract data pointer (field 0) and vtable pointer (field 1)
                llvm::Value* dataPtr = builder.CreateExtractValue(fatPtr, 0, "dyn_data_ptr");
                llvm::Value* vtablePtr = builder.CreateExtractValue(fatPtr, 1, "dyn_vtable_ptr");

                // Look up the trait's vtable struct type to get method index
                // The vtable struct type is named "TraitName_vtable_t"
                std::string vtableTyName = traitName + "_vtable_t";
                llvm::StructType* vtableStructTy = llvm::StructType::getTypeByName(context, vtableTyName);
                if (!vtableStructTy) {
                    throw std::runtime_error("No vtable type found for trait '" + traitName + "'");
                }

                // Find the method's index in the vtable using trait method ordering
                unsigned methodIndex = 0;
                bool foundMethod = false;

                if (trait_method_order_) {
                    auto it = trait_method_order_->find(traitName);
                    if (it != trait_method_order_->end()) {
                        for (unsigned i = 0; i < it->second.size(); i++) {
                            if (it->second[i] == methodName) {
                                methodIndex = i;
                                foundMethod = true;
                                break;
                            }
                        }
                    }
                }

                if (!foundMethod) {
                    throw std::runtime_error("Method '" + methodName +
                                             "' not found in trait '" + traitName + "'");
                }

                // Load method function pointer from vtable
                llvm::Value* methodGEP = builder.CreateStructGEP(
                    vtableStructTy, vtablePtr, methodIndex, "vtable_slot");
                llvm::Value* methodFnPtr = builder.CreateLoad(ptrTy, methodGEP, "dyn_method_ptr");

                // Build argument list: data_ptr as first arg, then explicit args
                std::vector<llvm::Value*> dynArgs;
                dynArgs.push_back(dataPtr);  // self data pointer
                for (size_t ai = 0; ai < expr->arguments.size(); ai++) {
                    llvm::Value* argVal = codegenExpressionNode(expr->arguments[ai].get(), this);
                    if (!argVal) {
                        throw std::runtime_error("Failed to generate dyn dispatch argument " + std::to_string(ai));
                    }
                    dynArgs.push_back(argVal);
                }

                // Build function type for indirect call
                // Thunk signature: Result<RetType>(ptr self_data, ...extra_args)
                // For now, look up the thunk's function type from any existing thunk
                llvm::FunctionType* thunkFT = nullptr;
                for (auto& fn : *module) {
                    std::string fnName = fn.getName().str();
                    std::string prefix = "__dyn_" + traitName + "_";
                    std::string suffix = "_" + methodName;
                    if (fnName.find(prefix) == 0 && fnName.size() > suffix.size() &&
                        fnName.substr(fnName.size() - suffix.size()) == suffix) {
                        thunkFT = fn.getFunctionType();
                        break;
                    }
                }
                if (!thunkFT) {
                    throw std::runtime_error("No thunk found for " + traitName + "::" + methodName);
                }

                llvm::Value* result = builder.CreateCall(thunkFT, methodFnPtr, dynArgs, "dyn_call");

                // The thunk returns Result<T> = { T, ptr, i1 }
                // Extract the actual value (field 0) so callers get the unwrapped value,
                // just like normal function calls that go through Result unwrapping.
                llvm::Value* unwrapped = builder.CreateExtractValue(result, 0, "dyn_result_val");

                ARIA_DBG_STREAM << "[DYN] Dispatched " << traitName << "::" << methodName
                          << " via vtable (index " << methodIndex << ")\n";
                return unwrapped;
            }

            mangled_func_name = type_name + "_" + member_access->member;

            // Inject the object as the first argument (UFCS transformation)
            std::vector<ASTNodePtr> new_args;
            new_args.push_back(member_access->object);  // Add object as first arg
            for (auto& arg : expr->arguments) {
                new_args.push_back(arg);
            }
            call_args = new_args;
            } else {
                // Static method call: Type.method() -> Type_method()
                mangled_func_name = ident->name + "_" + member_access->member;
            }
        }

        // Create a temporary IdentifierExpr with the mangled name
        // and recursively call codegenCall to handle it as a regular function
        auto temp_ident = std::make_shared<IdentifierExpr>(mangled_func_name, expr->line, expr->column);
        CallExpr temp_call(temp_ident, call_args, expr->line, expr->column);

        // Recursively generate code for the call with the mangled name
        return codegenCall(&temp_call);
    }
    
    // The callee should be an identifier (function name or func variable)
    IdentifierExpr* callee_ident = dynamic_cast<IdentifierExpr*>(expr->callee.get());
    if (!callee_ident) {
        ARIA_DBG_STREAM << "[DEBUG] Callee is not an IdentifierExpr! Type: " << static_cast<int>(expr->callee->type) << std::endl;
        throw std::runtime_error("Function callee must be an identifier or module member access");
    }
    
    // ====================================================================
    // BUILTIN FUNCTION: print() - Minimal Core Primitive
    // ====================================================================
    // Design Philosophy: Core language provides minimal, safe primitives.
    // Type-to-string conversions belong in stdlib (to_string() overloads).
    // Complex formatting via printf FFI or future stdlib printf-like function.
    // This keeps core simple, debuggable, and forces explicit formatting choices.
    
    // ====================================================================
    // BUILTIN FUNCTION: ok() - Unknown State Detection (Phase 5.2 + Layer 1 Safety)
    // ====================================================================
    // ok(value) -> int32 - Check if value is Unknown
    // Purpose: Detect Unknown sentinel values from divide-by-zero, overflow, etc.
    // Returns: 1 if value is valid, 0 if value is Unknown
    // Unknown sentinel = signed maximum value (INT_MAX for given bit width)
    
    if (callee_ident->name == "ok") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("ok() requires exactly one argument");
        }

        // ok(value) -> int32: returns 1 if value is valid, 0 if value is Unknown
        // Unknown sentinel = signed maximum value (INT_MAX for given bit width)
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg) {
            throw std::runtime_error("Failed to generate code for ok() argument");
        }
        
        if (arg->getType()->isIntegerTy()) {
            llvm::IntegerType* intType = llvm::cast<llvm::IntegerType>(arg->getType());
            unsigned width = intType->getBitWidth();
            llvm::Value* sentinel = llvm::ConstantInt::get(context, llvm::APInt::getSignedMaxValue(width));
            // Compare: is the value the unknown sentinel?
            llvm::Value* isUnknown = builder.CreateICmpEQ(arg, sentinel, "ok.isunk");
            // Return 0 if unknown, 1 if valid (as i32)
            llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
            llvm::Value* one = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1);
            return builder.CreateSelect(isUnknown, zero, one, "ok.result");
        }
        
        // Non-integer types: always valid
        ARIA_DBG_STREAM << "[DEBUG] ok() on non-integer type - always valid" << std::endl;
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 1);
    }
    
    // ====================================================================
    // SIMD HORIZONTAL REDUCTIONS (P1-2 Phase 5)
    // ====================================================================
    // Reduce SIMD vector to scalar using LLVM reduction intrinsics
    // Maps directly to hardware instructions (SSE, AVX, AVX-512)
    
    if (callee_ident->name == "@simd_sum" || callee_ident->name == "simd_sum") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("@simd_sum() requires exactly one SIMD vector argument");
        }
        
        llvm::Value* vec = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!vec || !vec->getType()->isVectorTy()) {
            throw std::runtime_error("@simd_sum() argument must be a SIMD vector");
        }
        
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(vec->getType());
        llvm::Type* elemType = vecType->getElementType();
        
        // Use appropriate reduction based on element type
        if (elemType->isFloatingPointTy()) {
            // Float reduction: llvm.vector.reduce.fadd
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_fadd, {vecType});
            llvm::Value* zero = llvm::ConstantFP::get(elemType, 0.0);
            return builder.CreateCall(reduceFunc, {zero, vec}, "simd.sum");
        } else {
            // Integer reduction: llvm.vector.reduce.add
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_add, {vecType});
            return builder.CreateCall(reduceFunc, {vec}, "simd.sum");
        }
    }
    
    if (callee_ident->name == "@simd_product" || callee_ident->name == "simd_product") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("@simd_product() requires exactly one SIMD vector argument");
        }
        
        llvm::Value* vec = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!vec || !vec->getType()->isVectorTy()) {
            throw std::runtime_error("@simd_product() argument must be a SIMD vector");
        }
        
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(vec->getType());
        llvm::Type* elemType = vecType->getElementType();
        
        // Use appropriate reduction based on element type
        if (elemType->isFloatingPointTy()) {
            // Float reduction: llvm.vector.reduce.fmul
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_fmul, {vecType});
            llvm::Value* one = llvm::ConstantFP::get(elemType, 1.0);
            return builder.CreateCall(reduceFunc, {one, vec}, "simd.product");
        } else {
            // Integer reduction: llvm.vector.reduce.mul
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_mul, {vecType});
            return builder.CreateCall(reduceFunc, {vec}, "simd.product");
        }
    }
    
    if (callee_ident->name == "@simd_min" || callee_ident->name == "simd_min") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("@simd_min() requires exactly one SIMD vector argument");
        }
        
        llvm::Value* vec = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!vec || !vec->getType()->isVectorTy()) {
            throw std::runtime_error("@simd_min() argument must be a SIMD vector");
        }
        
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(vec->getType());
        llvm::Type* elemType = vecType->getElementType();
        
        // Use appropriate reduction based on element type
        if (elemType->isFloatingPointTy()) {
            // Float min: llvm.vector.reduce.fmin
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_fmin, {vecType});
            return builder.CreateCall(reduceFunc, {vec}, "simd.min");
        } else {
            // Signed integer min: llvm.vector.reduce.smin
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_smin, {vecType});
            return builder.CreateCall(reduceFunc, {vec}, "simd.min");
        }
    }
    
    if (callee_ident->name == "@simd_max" || callee_ident->name == "simd_max") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("@simd_max() requires exactly one SIMD vector argument");
        }
        
        llvm::Value* vec = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!vec || !vec->getType()->isVectorTy()) {
            throw std::runtime_error("@simd_max() argument must be a SIMD vector");
        }
        
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(vec->getType());
        llvm::Type* elemType = vecType->getElementType();
        
        // Use appropriate reduction based on element type
        if (elemType->isFloatingPointTy()) {
            // Float max: llvm.vector.reduce.fmax
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_fmax, {vecType});
            return builder.CreateCall(reduceFunc, {vec}, "simd.max");
        } else {
            // Signed integer max: llvm.vector.reduce.smax
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_smax, {vecType});
            return builder.CreateCall(reduceFunc, {vec}, "simd.max");
        }
    }
    
    // ====================================================================
    // SIMD Boolean Operations (P1-2 Phase 6)
    // ====================================================================
    // Boolean reductions for SIMD mask operations
    
    if (callee_ident->name == "@simd_any" || callee_ident->name == "simd_any") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("simd_any() requires exactly one SIMD boolean vector argument");
        }
        
        llvm::Value* vec = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!vec || !vec->getType()->isVectorTy()) {
            throw std::runtime_error("simd_any() argument must be a SIMD vector");
        }
        
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(vec->getType());
        
        // Use llvm.vector.reduce.or to check if ANY bit is set
        // For bool vectors (i1), this gives us true if any lane is true
        llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::vector_reduce_or, {vecType});
        return builder.CreateCall(reduceFunc, {vec}, "simd.any");
    }
    
    if (callee_ident->name == "@simd_all" || callee_ident->name == "simd_all") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("simd_all() requires exactly one SIMD boolean vector argument");
        }
        
        llvm::Value* vec = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!vec || !vec->getType()->isVectorTy()) {
            throw std::runtime_error("simd_all() argument must be a SIMD vector");
        }
        
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(vec->getType());
        
        // Use llvm.vector.reduce.and to check if ALL bits are set
        // For bool vectors (i1), this gives us true if all lanes are true
        llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::vector_reduce_and, {vecType});
        return builder.CreateCall(reduceFunc, {vec}, "simd.all");
    }
    
    // simd_select(mask, true_vals, false_vals) - Masked SIMD selection
    // Per-lane ternary: returns true_vals[i] if mask[i] else false_vals[i]
    // Maps directly to LLVM's select instruction for branchless conditional operations
    if (callee_ident->name == "@simd_select" || callee_ident->name == "simd_select") {
        if (expr->arguments.size() != 3) {
            throw std::runtime_error("simd_select() requires exactly three arguments (mask, true_vals, false_vals)");
        }
        
        llvm::Value* mask = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* trueVals = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* falseVals = codegenExpressionNode(expr->arguments[2].get(), this);
        
        if (!mask || !trueVals || !falseVals) {
            throw std::runtime_error("Failed to generate code for simd_select arguments");
        }
        
        // LLVM select: per-lane conditional selection (branchless)
        // For each lane i: result[i] = mask[i] ? trueVals[i] : falseVals[i]
        return builder.CreateSelect(mask, trueVals, falseVals, "simd.select");
    }
    
    // simd_broadcast(scalar, lanes) - Broadcast scalar to all SIMD lanes
    // Creates a uniform SIMD vector where all lanes contain the same value
    if (callee_ident->name == "@simd_broadcast" || callee_ident->name == "simd_broadcast") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("simd_broadcast() requires exactly two arguments (value, lane_count)");
        }
        
        llvm::Value* scalarValue = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!scalarValue) {
            throw std::runtime_error("Failed to generate code for simd_broadcast scalar value");
        }
        
        // Extract lane count from integer literal
        if (expr->arguments[1]->type != aria::ASTNode::NodeType::LITERAL) {
            throw std::runtime_error("simd_broadcast() lane count must be a compile-time integer literal");
        }
        aria::LiteralExpr* laneCountLit = static_cast<aria::LiteralExpr*>(expr->arguments[1].get());
        if (!std::holds_alternative<int64_t>(laneCountLit->value)) {
            throw std::runtime_error("simd_broadcast() lane count must be an integer");
        }
        unsigned laneCount = std::get<int64_t>(laneCountLit->value);
        
        // Get the scalar type
        llvm::Type* scalarType = scalarValue->getType();
        
        // Create vector type
        llvm::VectorType* vecType = llvm::VectorType::get(scalarType, laneCount, false);
        
        // Method 1: Use a splat constant (works for constants)
        // Method 2: Insert into undef, then shuffle (works for all values)
        
        // Create undef vector
        llvm::Value* undefVec = llvm::UndefValue::get(vecType);
        
        // Insert scalar at position 0
        llvm::Value* vec0 = builder.CreateInsertElement(undefVec, scalarValue, 
                                                        uint64_t(0), "broadcast.insert");
        
        // Create shuffle mask: all zeros (broadcast lane 0 to all lanes)
        std::vector<int> shuffleMask(laneCount, 0);
        
        // Shuffle to broadcast
        llvm::Value* result = builder.CreateShuffleVector(vec0, undefVec, shuffleMask, "simd.broadcast");
        
        return result;
    }
    
    // simd_load(ptr, lanes) - Load SIMD vector from aligned memory
    // Loads N elements from memory into a SIMD vector
    if (callee_ident->name == "@simd_load" || callee_ident->name == "simd_load") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("simd_load() requires exactly two arguments (pointer, lane_count)");
        }
        
        llvm::Value* ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!ptr || !ptr->getType()->isPointerTy()) {
            throw std::runtime_error("simd_load() first argument must be a pointer");
        }
        
        // Extract lane count from literal
        if (expr->arguments[1]->type != aria::ASTNode::NodeType::LITERAL) {
            throw std::runtime_error("simd_load() lane count must be a compile-time integer literal");
        }
        aria::LiteralExpr* laneCountLit = static_cast<aria::LiteralExpr*>(expr->arguments[1].get());
        if (!std::holds_alternative<int64_t>(laneCountLit->value)) {
            throw std::runtime_error("simd_load() lane count must be an integer");
        }
        unsigned laneCount = std::get<int64_t>(laneCountLit->value);
        
        // For LLVM 20 opaque pointers, we need to determine element type from Aria type or context
        // Get the Aria type of the pointer argument
        std::string ptrTypeName = "unknown";
        if (var_aria_types.count(expr->arguments[0]->toString())) {
            ptrTypeName = var_aria_types[expr->arguments[0]->toString()];
        }
        
        // For now, default to int32 if we can't determine the type
        // In practice, the type will be known from the function signature
        llvm::Type* elementType = builder.getInt32Ty();  // Default
        
        // Try to extract element type from pointer type annotation if available
        // This is a simplified approach - full implementation would query the type system
        if (ptrTypeName.find("int32*") != std::string::npos) {
            elementType = builder.getInt32Ty();
        } else if (ptrTypeName.find("float32*") != std::string::npos || ptrTypeName.find("flt32*") != std::string::npos) {
            elementType = builder.getFloatTy();
        } else if (ptrTypeName.find("int64*") != std::string::npos) {
            elementType = builder.getInt64Ty();
        } else if (ptrTypeName.find("float64*") != std::string::npos || ptrTypeName.find("flt64*") != std::string::npos) {
            elementType = builder.getDoubleTy();
        }
        // Add more type mappings as needed
        
        // Create vector type
        llvm::VectorType* vecType = llvm::VectorType::get(elementType, laneCount, false);
        
        // Load the vector using modern LLVM load instruction
        llvm::LoadInst* loadInst = builder.CreateLoad(vecType, ptr, "simd.load");
        
        // Set alignment (element size for natural alignment)
        unsigned elementSizeBytes = elementType->getPrimitiveSizeInBits() / 8;
        loadInst->setAlignment(llvm::Align(elementSizeBytes));
        
        return loadInst;
    }
    
    // simd_store(ptr, vec) - Store SIMD vector to aligned memory
    // Stores all elements of a SIMD vector to memory
    if (callee_ident->name == "@simd_store" || callee_ident->name == "simd_store") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("simd_store() requires exactly two arguments (pointer, vector)");
        }
        
        llvm::Value* ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* vec = codegenExpressionNode(expr->arguments[1].get(), this);
        
        if (!ptr || !ptr->getType()->isPointerTy()) {
            throw std::runtime_error("simd_store() first argument must be a pointer");
        }
        
        if (!vec || !vec->getType()->isVectorTy()) {
            throw std::runtime_error("simd_store() second argument must be a SIMD vector");
        }
        
        // Get vector type info
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(vec->getType());
        llvm::Type* elementType = vecType->getElementType();
        
        // Store the vector using modern LLVM store instruction
        llvm::StoreInst* storeInst = builder.CreateStore(vec, ptr);
        
        // Set alignment (element size for natural alignment)
        unsigned elementSizeBytes = elementType->getPrimitiveSizeInBits() / 8;
        storeInst->setAlignment(llvm::Align(elementSizeBytes));
        
        // Return void (store instruction itself, but it's unused)
        return storeInst;
    }
    
    // print(string) - write string as-is (no newline)
    // println(string) - write string + newline (convenience)
    // Both minimal primitives with same interface, different intent
    if (callee_ident->name == "print" || callee_ident->name == "println") {
        bool add_newline = (callee_ident->name == "println");
        
        if (expr->arguments.size() != 1) {
            throw std::runtime_error(callee_ident->name + "() requires exactly one string argument");
        }
        
        // Evaluate the argument
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg) {
            throw std::runtime_error("Failed to generate code for print argument");
        }
        
        // Auto-unwrap Result<T> = { T, ptr, i8 } from user-defined function calls.
        // All Aria user functions return a Result struct; extract field 0 (the value).
        if (arg->getType()->isStructTy()) {
            llvm::StructType* struct_type = llvm::cast<llvm::StructType>(arg->getType());
            if (struct_type->getNumElements() == 3 &&
                struct_type->getElementType(1)->isPointerTy() &&
                struct_type->getElementType(2)->isIntegerTy(8)) {
                arg = builder.CreateExtractValue(arg, {0}, "result_val");
            }
        }
        
        llvm::Value* str_ptr = nullptr;
        
        // Check if argument is an AriaString struct (not a pointer, but the struct itself)
        // String literals return GlobalVariable pointers to AriaString structs
        if (llvm::isa<llvm::GlobalVariable>(arg)) {
            llvm::GlobalVariable* gv = llvm::cast<llvm::GlobalVariable>(arg);
            llvm::Type* gv_type = gv->getValueType();
            
            if (gv_type->isStructTy()) {
                llvm::StructType* struct_ty = llvm::cast<llvm::StructType>(gv_type);
                std::string struct_name = struct_ty->hasName() ? struct_ty->getName().str() : "";
                
                if (struct_name == "struct.AriaString") {
                    // Extract the data field (char* at index 0) from the global
                    llvm::Value* data_gep = builder.CreateStructGEP(
                        struct_ty,
                        arg,
                        0,  // data field is first
                        "str_data_ptr"
                    );
                    str_ptr = builder.CreateLoad(
                        llvm::PointerType::get(context, 0),
                        data_gep,
                        "str_data"
                    );
                } else {
                    throw std::runtime_error("print() cannot print this struct type");
                }
            } else {
                // Global non-struct - assume it's a raw string
                str_ptr = arg;
            }
        } else if (arg->getType()->isPointerTy()) {
            // Handle AriaString* pointers from string variables and runtime functions
            // (e.g. string_from_int, string_concat return AriaString*)
            // We need to load the AriaString struct and extract the char* data field.

            // Get or create AriaString struct type { i8*, i64 }
            // Must always create it here if missing — e.g. when there are no string
            // literals in the program so the type was never added to the module.
            llvm::StructType* ariaStringType = llvm::StructType::getTypeByName(context, "struct.AriaString");
            if (!ariaStringType) {
                ariaStringType = llvm::StructType::create(context, {
                    llvm::PointerType::get(context, 0),   // data: char*
                    llvm::Type::getInt64Ty(context)        // length: int64
                }, "struct.AriaString");
            }

            // Load the AriaString struct from the pointer, then extract data field
            llvm::Value* str_struct = builder.CreateLoad(ariaStringType, arg, "str_struct");
            str_ptr = builder.CreateExtractValue(str_struct, 0, "str_data");
        } else {
            std::string type_str;
            llvm::raw_string_ostream rso(type_str);
            arg->getType()->print(rso);
            rso.flush();
            throw std::runtime_error(
                callee_ident->name + "() requires a string argument. "
                "For other types, use to_string() from stdlib or printf via FFI."
                " [got LLVM type: " + type_str + "]"
            );
        }
        
        // Declare runtime function: aria_print_cstr or aria_println_cstr
        // Signature: int64_t aria_print[ln]_cstr(const char* str)
        // Returns: Number of bytes written, or -1 on error
        const char* func_name = add_newline ? "aria_println_cstr" : "aria_print_cstr";
        llvm::Function* aria_print = module->getFunction(func_name);
        if (!aria_print) {
            llvm::FunctionType* print_type = llvm::FunctionType::get(
                builder.getInt64Ty(),                    // returns bytes written
                {llvm::PointerType::get(context, 0)},    // takes const char*
                false                                     // not vararg
            );
            aria_print = llvm::Function::Create(
                print_type,
                llvm::Function::ExternalLinkage,
                func_name,
                module
            );
        }
        
        // Call aria_print[ln]_cstr(str) and return result
        return builder.CreateCall(aria_print, {str_ptr}, "print_call");
    }
    
    // ====================================================================
    // BUILTIN FUNCTIONS: 6-Stream I/O System
    // ====================================================================
    
    // Helper lambda to create stream write functions
    auto create_stream_write = [&](const std::string& func_name, const std::string& stream_func) -> llvm::Value* {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error(func_name + " requires exactly one argument");
        }
        
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg) {
            throw std::runtime_error("Failed to generate code for " + func_name + " argument");
        }
        
        // Ensure argument is a string
        if (!arg->getType()->isPointerTy()) {
            throw std::runtime_error(func_name + " requires a string argument");
        }

        // Extract the char* data pointer from the AriaString* pointer.
        // All Aria strings are AriaString structs { char* data, int64_t length }.
        // The C runtime stream functions expect a raw const char*.
        llvm::StructType* ariaStringType = llvm::StructType::getTypeByName(context, "struct.AriaString");
        if (!ariaStringType) {
            ariaStringType = llvm::StructType::create(context, {
                llvm::PointerType::get(context, 0),
                llvm::Type::getInt64Ty(context)
            }, "struct.AriaString");
        }

        llvm::Value* str_ptr = nullptr;
        if (llvm::isa<llvm::GlobalVariable>(arg)) {
            // String literal: GEP into the global AriaString to get .data
            llvm::Value* data_gep = builder.CreateStructGEP(ariaStringType, arg, 0, "sw_data_ptr");
            str_ptr = builder.CreateLoad(llvm::PointerType::get(context, 0), data_gep, "sw_data");
        } else {
            // AriaString* from variable or runtime call: load struct, extract .data
            llvm::Value* str_struct = builder.CreateLoad(ariaStringType, arg, "sw_str_struct");
            str_ptr = builder.CreateExtractValue(str_struct, 0, "sw_data");
        }
        
        // Declare the C runtime function if not already declared
        // Signature: int64_t aria_xxx_write(const char* str)
        llvm::Function* stream_func_ptr = module->getFunction(stream_func);
        if (!stream_func_ptr) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(),  // returns int64_t
                {llvm::PointerType::get(context, 0)},  // takes const char*
                false  // not vararg
            );
            stream_func_ptr = llvm::Function::Create(
                func_type,
                llvm::Function::ExternalLinkage,
                stream_func,
                module
            );
        }
        
        // Call the stream function with the extracted char* data pointer
        return builder.CreateCall(stream_func_ptr, {str_ptr}, func_name + "_call");
    };
    
    // stdout_write(string) -> int64
    if (callee_ident->name == "stdout_write") {
        return create_stream_write("stdout_write", "aria_stdout_write");
    }
    
    // stderr_write(string) -> int64
    if (callee_ident->name == "stderr_write") {
        return create_stream_write("stderr_write", "aria_stderr_write");
    }
    
    // stddbg_write(string) -> int64
    if (callee_ident->name == "stddbg_write") {
        return create_stream_write("stddbg_write", "aria_stddbg_write");
    }
    
    // ====================================================================
    // SYSCALL BUILTINS — sys() / sys!!() / sys!!!()
    // Emits x86-64 Linux syscall via LLVM inline assembly.
    // ====================================================================
    if (callee_ident->name == "sys" || callee_ident->name == "sys!!" || callee_ident->name == "sys!!!") {
        bool isRaw = (callee_ident->name == "sys!!!");
        
        // Map syscall name constants to x86-64 Linux syscall numbers
        static const std::unordered_map<std::string, int64_t> syscallNumbers = {
            // File I/O
            {"READ", 0}, {"WRITE", 1}, {"OPEN", 2}, {"CLOSE", 3},
            {"STAT", 4}, {"FSTAT", 5}, {"LSTAT", 6},
            {"LSEEK", 8}, {"PREAD64", 17}, {"PWRITE64", 18},
            {"READV", 19}, {"WRITEV", 20},
            {"ACCESS", 21}, {"PIPE2", 293},
            {"OPENAT", 257}, {"NEWFSTATAT", 262},
            // Memory
            {"MMAP", 9}, {"MPROTECT", 10}, {"MUNMAP", 11}, {"BRK", 12}, {"MREMAP", 25},
            {"MADVISE", 28}, {"MINCORE", 27},
            {"MLOCK", 149}, {"MUNLOCK", 150}, {"MLOCKALL", 151}, {"MUNLOCKALL", 152},
            // Directory
            {"GETDENTS64", 217}, {"GETCWD", 79}, {"CHDIR", 80},
            {"MKDIR", 83}, {"RMDIR", 84}, {"RENAME", 82},
            {"RENAMEAT2", 316}, {"UNLINK", 87}, {"UNLINKAT", 263},
            {"SYMLINK", 88}, {"READLINK", 89},
            {"MKDIRAT", 258}, {"MKNODAT", 259}, {"SYMLINKAT", 265}, {"READLINKAT", 267}, {"LINKAT", 265},
            // File metadata
            {"FCHMOD", 91}, {"FCHOWN", 93}, {"UTIMENSAT", 280}, {"FACCESSAT", 269},
            {"CHMOD", 90}, {"CHOWN", 92}, {"LCHOWN", 94},
            {"LINK", 86}, {"TRUNCATE", 76}, {"FTRUNCATE", 77},
            {"STATX", 332},
            // Process info
            {"GETPID", 39}, {"GETPPID", 110}, {"GETTID", 186},
            {"GETUID", 102}, {"GETGID", 104}, {"GETEUID", 107}, {"GETEGID", 108},
            // Time
            {"CLOCK_GETTIME", 228}, {"CLOCK_NANOSLEEP", 230}, {"NANOSLEEP", 35},
            // Networking
            {"SOCKET", 41}, {"BIND", 49}, {"LISTEN", 50}, {"ACCEPT4", 288},
            {"CONNECT", 42}, {"SEND", 44}, {"RECV", 45},
            {"SENDTO", 44}, {"RECVFROM", 45},
            {"SETSOCKOPT", 54}, {"GETSOCKOPT", 55}, {"SHUTDOWN", 48},
            {"SENDMSG", 46}, {"RECVMSG", 47}, {"SENDMMSG", 307}, {"RECVMMSG", 299},
            // Polling
            {"POLL", 7}, {"PPOLL", 271},
            {"EPOLL_CREATE1", 291}, {"EPOLL_CTL", 233}, {"EPOLL_WAIT", 232},
            {"SELECT", 23}, {"PSELECT6", 270},
            // Pipe / IPC / fd ops
            {"DUP", 32}, {"DUP2", 33}, {"DUP3", 292}, {"EVENTFD2", 290},
            {"IOCTL", 16}, {"FCNTL", 72}, {"FLOCK", 73}, {"FSYNC", 74}, {"FDATASYNC", 75},
            {"GETRANDOM", 318},
            // Dangerous (require sys!! or sys!!!)
            {"EXIT", 60}, {"EXIT_GROUP", 231},
            {"KILL", 62}, {"TKILL", 200}, {"TGKILL", 234},
            {"EXECVE", 59}, {"EXECVEAT", 322},
            {"FORK", 57}, {"CLONE", 56}, {"CLONE3", 435}, {"VFORK", 58},
            {"PTRACE", 101},
            {"MOUNT", 165}, {"UMOUNT2", 166},
            {"REBOOT", 169},
            {"SETUID", 105}, {"SETGID", 106}, {"SETREUID", 113}, {"SETREGID", 114},
            {"SETEUID", 117}, {"SETEGID", 119},
            {"INIT_MODULE", 175}, {"DELETE_MODULE", 176}, {"FINIT_MODULE", 313},
            {"WAIT4", 61}, {"WAITID", 247},
            {"PRCTL", 157}, {"ARCH_PRCTL", 158},
            {"SECCOMP", 317}, {"USERFAULTFD", 323}, {"PERF_EVENT_OPEN", 298},
            {"SIGNALFD4", 289}, {"TIMERFD_CREATE", 283},
            {"TIMERFD_SETTIME", 286}, {"TIMERFD_GETTIME", 287},
            {"RT_SIGACTION", 13}, {"RT_SIGPROCMASK", 14},
            {"RT_SIGRETURN", 15}, {"RT_SIGPENDING", 127}, {"SIGALTSTACK", 131},
            {"SETXATTR", 188}, {"GETXATTR", 191}, {"REMOVEXATTR", 197},
            {"LSETXATTR", 189}, {"LGETXATTR", 192}, {"LREMOVEXATTR", 198},
            {"FSETXATTR", 190}, {"FGETXATTR", 193}, {"FREMOVEXATTR", 199},
            {"LISTXATTR", 194}, {"LLISTXATTR", 195}, {"FLISTXATTR", 196},
            {"IO_URING_SETUP", 425}, {"IO_URING_ENTER", 426}, {"IO_URING_REGISTER", 427},
            {"SCHED_YIELD", 24}, {"SCHED_GETAFFINITY", 204}, {"SCHED_SETAFFINITY", 203},
            {"FUTEX", 202},
            {"SET_TID_ADDRESS", 218}, {"SET_ROBUST_LIST", 273}, {"GET_ROBUST_LIST", 274},
            {"SYSINFO", 99}, {"UNAME", 63},
            {"GETRLIMIT", 97}, {"SETRLIMIT", 160}, {"PRLIMIT64", 302},
            {"GETRUSAGE", 98}, {"SYSLOG", 103},
            {"INOTIFY_INIT1", 294}, {"INOTIFY_ADD_WATCH", 254}, {"INOTIFY_RM_WATCH", 255}
        };
        
        // x86-64 register names for syscall arguments (after rax for the number)
        static const char* const regConstraints[] = {
            "{rdi}", "{rsi}", "{rdx}", "{r10}", "{r8}", "{r9}"
        };
        
        size_t numArgs = expr->arguments.size();  // includes syscall number
        size_t numSyscallArgs = numArgs - 1;      // actual args to the syscall
        
        // --- Resolve syscall number ---
        llvm::Value* syscallNr = nullptr;
        IdentifierExpr* syscallId = dynamic_cast<IdentifierExpr*>(expr->arguments[0].get());
        if (syscallId) {
            auto it = syscallNumbers.find(syscallId->name);
            if (it != syscallNumbers.end()) {
                syscallNr = builder.getInt64(it->second);
            }
        }
        if (!syscallNr) {
            // Not a named constant — evaluate as expression
            syscallNr = codegenExpressionNode(expr->arguments[0].get(), this);
            if (!syscallNr) {
                throw std::runtime_error("Failed to generate syscall number");
            }
            // Cast to i64 if needed
            if (syscallNr->getType() != builder.getInt64Ty()) {
                if (syscallNr->getType()->isIntegerTy()) {
                    syscallNr = builder.CreateZExtOrTrunc(syscallNr, builder.getInt64Ty(), "sys_nr_cast");
                } else {
                    throw std::runtime_error("Syscall number must be an integer type");
                }
            }
        }
        
        // ================================================================
        // STRING_BUF SYSCALLS — typed returns (v0.4.1)
        // GETCWD and READLINK write a string into a user buffer.
        // The compiler manages the buffer and returns Result<string>.
        // ================================================================
        if (syscallId && !isRaw) {
            if (syscallId->name == "GETCWD" || syscallId->name == "READLINK") {
                // All STRING_BUF syscalls follow this pattern:
                //   1. mmap anonymous buffer (4096 bytes)
                //   2. Issue syscall with buffer as appropriate arg
                //   3. On success: strdup(buf) → Aria string
                //   4. munmap buffer
                //   5. Return Result<string>
                
                llvm::Type* i64Ty = builder.getInt64Ty();
                llvm::Type* ptrTy = llvm::PointerType::get(context, 0);
                llvm::Type* i8Ty = builder.getInt8Ty();
                llvm::Value* bufSize = builder.getInt64(4096);
                
                // --- Step 1: mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0) ---
                // mmap is syscall 9
                std::vector<llvm::Type*> mmapParamTypes(7, i64Ty);
                llvm::FunctionType* mmapAsmType = llvm::FunctionType::get(i64Ty, mmapParamTypes, false);
                llvm::InlineAsm* mmapAsm = llvm::InlineAsm::get(
                    mmapAsmType, "syscall",
                    "={rax},{rax},{rdi},{rsi},{rdx},{r10},{r8},{r9},~{rcx},~{r11},~{memory}",
                    true, false, llvm::InlineAsm::AD_ATT);
                
                llvm::Value* mmapResult = builder.CreateCall(mmapAsmType, mmapAsm, {
                    builder.getInt64(9),    // SYS_MMAP
                    builder.getInt64(0),    // addr = NULL
                    bufSize,                // len = 4096
                    builder.getInt64(3),    // prot = PROT_READ|PROT_WRITE
                    builder.getInt64(34),   // flags = MAP_PRIVATE|MAP_ANONYMOUS
                    builder.getInt64(-1),   // fd = -1
                    builder.getInt64(0)     // offset = 0
                }, "sys_strbuf_mmap");
                
                // Check mmap success (result should be positive)
                llvm::Value* mmapFailed = builder.CreateICmpSLT(mmapResult, builder.getInt64(1), "sys_mmap_fail");
                
                // We need basic blocks for mmap success/fail
                llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();
                llvm::BasicBlock* mmapOkBB = llvm::BasicBlock::Create(context, "sys_strbuf_mmap_ok", currentFunc);
                llvm::BasicBlock* mmapFailBB = llvm::BasicBlock::Create(context, "sys_strbuf_mmap_fail", currentFunc);
                llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "sys_strbuf_merge", currentFunc);
                
                builder.CreateCondBr(mmapFailed, mmapFailBB, mmapOkBB);
                
                // --- mmap failed path: return error Result<string> ---
                builder.SetInsertPoint(mmapFailBB);
                // Create empty string for error case
                llvm::StructType* ariaStrType = llvm::StructType::getTypeByName(context, "struct.AriaString");
                if (!ariaStrType) {
                    ariaStrType = llvm::StructType::create(context, {ptrTy, i64Ty}, "struct.AriaString");
                }
                // Result<string> = { AriaString* value, ptr error, i8 is_error }
                // For string results, use AriaString pointer as value
                llvm::StructType* resultStrTy = llvm::StructType::get(context, {ptrTy, ptrTy, i8Ty});
                
                // Alloca at function entry for Result<string> — LLVM mem2reg optimizes to PHI
                llvm::BasicBlock& entryBlock = currentFunc->getEntryBlock();
                llvm::IRBuilder<> entryBuilder(&entryBlock, entryBlock.begin());
                llvm::Value* resultAlloca = entryBuilder.CreateAlloca(resultStrTy, nullptr, "sys_strbuf_result");
                
                // Create empty string constant for error path
                llvm::Constant* emptyStrData = builder.CreateGlobalStringPtr("", "sys_strbuf_empty_data");
                llvm::Value* emptyStr_fail = llvm::UndefValue::get(ariaStrType);
                emptyStr_fail = builder.CreateInsertValue(emptyStr_fail, emptyStrData, 0, "es_fail.data");
                emptyStr_fail = builder.CreateInsertValue(emptyStr_fail, builder.getInt64(0), 1, "es_fail.len");
                
                // GC-alloc to hold the AriaString struct
                llvm::FunctionCallee gcAlloc = module->getOrInsertFunction("aria_gc_alloc",
                    llvm::FunctionType::get(ptrTy, {i64Ty}, false));
                llvm::Value* emptyStrPtr_fail = builder.CreateCall(gcAlloc, {builder.getInt64(16)}, "es_fail_ptr");
                builder.CreateStore(emptyStr_fail, emptyStrPtr_fail);
                
                // Negate mmap result for errno
                llvm::Value* negMmap = builder.CreateNeg(mmapResult, "sys_strbuf_neg_errno");
                llvm::Value* errPtrFail = builder.CreateIntToPtr(negMmap, ptrTy, "sys_strbuf_err_ptr");
                
                llvm::Value* failResult = llvm::UndefValue::get(resultStrTy);
                failResult = builder.CreateInsertValue(failResult, emptyStrPtr_fail, 0, "sys_strbuf_fail.val");
                failResult = builder.CreateInsertValue(failResult, errPtrFail, 1, "sys_strbuf_fail.err");
                failResult = builder.CreateInsertValue(failResult, llvm::ConstantInt::get(i8Ty, 1), 2, "sys_strbuf_fail.flag");
                builder.CreateStore(failResult, resultAlloca);
                builder.CreateBr(mergeBB);
                
                // --- mmap succeeded path: issue the actual syscall ---
                builder.SetInsertPoint(mmapOkBB);
                
                llvm::Value* bufPtr = mmapResult; // buffer address as i64
                llvm::Value* sysRawResult = nullptr;
                
                if (syscallId->name == "GETCWD") {
                    // getcwd(buf, size) — syscall 79
                    std::vector<llvm::Type*> cwdParamTypes(3, i64Ty);
                    llvm::FunctionType* cwdAsmType = llvm::FunctionType::get(i64Ty, cwdParamTypes, false);
                    llvm::InlineAsm* cwdAsm = llvm::InlineAsm::get(
                        cwdAsmType, "syscall",
                        "={rax},{rax},{rdi},{rsi},~{rcx},~{r11},~{memory}",
                        true, false, llvm::InlineAsm::AD_ATT);
                    sysRawResult = builder.CreateCall(cwdAsmType, cwdAsm, {
                        syscallNr, bufPtr, bufSize
                    }, "sys_getcwd_ret");
                    
                } else if (syscallId->name == "READLINK") {
                    // readlink(path, buf, bufsiz) — syscall 89
                    // User provides 1 arg: the path
                    llvm::Value* pathArg = codegenExpressionNode(expr->arguments[1].get(), this);
                    if (!pathArg) {
                        throw std::runtime_error("Failed to generate READLINK path argument");
                    }
                    // Convert path to char* (i64 for register)
                    if (llvm::isa<llvm::GlobalVariable>(pathArg)) {
                        llvm::GlobalVariable* gv = llvm::cast<llvm::GlobalVariable>(pathArg);
                        llvm::Type* gvType = gv->getValueType();
                        if (gvType->isStructTy()) {
                            llvm::Value* dataGep = builder.CreateStructGEP(gvType, pathArg, 0, "sys_rl_data_ptr");
                            llvm::Value* dataPtr = builder.CreateLoad(ptrTy, dataGep, "sys_rl_data");
                            pathArg = builder.CreatePtrToInt(dataPtr, i64Ty, "sys_rl_str_int");
                        } else {
                            pathArg = builder.CreatePtrToInt(pathArg, i64Ty, "sys_rl_gv_int");
                        }
                    } else if (pathArg->getType()->isPointerTy()) {
                        llvm::StructType* asType = llvm::StructType::getTypeByName(context, "struct.AriaString");
                        if (asType) {
                            llvm::Value* strStruct = builder.CreateLoad(asType, pathArg, "sys_rl_str_struct");
                            llvm::Value* dataPtr = builder.CreateExtractValue(strStruct, 0, "sys_rl_str_data");
                            pathArg = builder.CreatePtrToInt(dataPtr, i64Ty, "sys_rl_strvar_int");
                        } else {
                            pathArg = builder.CreatePtrToInt(pathArg, i64Ty, "sys_rl_ptr_int");
                        }
                    }
                    
                    std::vector<llvm::Type*> rlParamTypes(4, i64Ty);
                    llvm::FunctionType* rlAsmType = llvm::FunctionType::get(i64Ty, rlParamTypes, false);
                    llvm::InlineAsm* rlAsm = llvm::InlineAsm::get(
                        rlAsmType, "syscall",
                        "={rax},{rax},{rdi},{rsi},{rdx},~{rcx},~{r11},~{memory}",
                        true, false, llvm::InlineAsm::AD_ATT);
                    sysRawResult = builder.CreateCall(rlAsmType, rlAsm, {
                        syscallNr, pathArg, bufPtr, bufSize
                    }, "sys_readlink_ret");
                }
                
                // Check syscall success
                llvm::Value* sysIsErr = builder.CreateICmpSLT(sysRawResult, builder.getInt64(0), "sys_strbuf_err");
                
                llvm::BasicBlock* sysOkBB = llvm::BasicBlock::Create(context, "sys_strbuf_ok", currentFunc);
                llvm::BasicBlock* sysErrBB = llvm::BasicBlock::Create(context, "sys_strbuf_syserr", currentFunc);
                
                builder.CreateCondBr(sysIsErr, sysErrBB, sysOkBB);
                
                // --- Syscall error path: munmap buffer + return error ---
                builder.SetInsertPoint(sysErrBB);
                {
                    // munmap(buf, 4096) — syscall 11
                    std::vector<llvm::Type*> munmapParamTypes(3, i64Ty);
                    llvm::FunctionType* munmapAsmType = llvm::FunctionType::get(i64Ty, munmapParamTypes, false);
                    llvm::InlineAsm* munmapAsm = llvm::InlineAsm::get(
                        munmapAsmType, "syscall",
                        "={rax},{rax},{rdi},{rsi},~{rcx},~{r11},~{memory}",
                        true, false, llvm::InlineAsm::AD_ATT);
                    builder.CreateCall(munmapAsmType, munmapAsm, {
                        builder.getInt64(11), bufPtr, bufSize
                    }, "sys_strbuf_munmap_err");
                    
                    // Build error Result<string>
                    llvm::Constant* emptyStrData2 = builder.CreateGlobalStringPtr("", "sys_strbuf_empty2");
                    llvm::Value* emptyStr2 = llvm::UndefValue::get(ariaStrType);
                    emptyStr2 = builder.CreateInsertValue(emptyStr2, emptyStrData2, 0, "es2.data");
                    emptyStr2 = builder.CreateInsertValue(emptyStr2, builder.getInt64(0), 1, "es2.len");
                    llvm::Value* emptyStrPtr2 = builder.CreateCall(gcAlloc, {builder.getInt64(16)}, "es2_ptr");
                    builder.CreateStore(emptyStr2, emptyStrPtr2);
                    
                    llvm::Value* negSys = builder.CreateNeg(sysRawResult, "sys_strbuf_neg_errno2");
                    llvm::Value* errPtrSys = builder.CreateIntToPtr(negSys, ptrTy, "sys_strbuf_err_ptr2");
                    
                    llvm::Value* sysErrResult = llvm::UndefValue::get(resultStrTy);
                    sysErrResult = builder.CreateInsertValue(sysErrResult, emptyStrPtr2, 0, "sys_strbuf_syserr.val");
                    sysErrResult = builder.CreateInsertValue(sysErrResult, errPtrSys, 1, "sys_strbuf_syserr.err");
                    sysErrResult = builder.CreateInsertValue(sysErrResult, llvm::ConstantInt::get(i8Ty, 1), 2, "sys_strbuf_syserr.flag");
                    builder.CreateStore(sysErrResult, resultAlloca);
                    builder.CreateBr(mergeBB);
                }
                
                // --- Syscall success path: strdup buffer → Aria string, munmap buffer ---
                builder.SetInsertPoint(sysOkBB);
                {
                    // Null-terminate: for READLINK, kernel doesn't null-terminate.
                    // GETCWD does null-terminate. For safety, always null-terminate at count position.
                    if (syscallId->name == "READLINK") {
                        // buf[count] = '\0'
                        llvm::Value* bufAddr = builder.CreateIntToPtr(bufPtr, ptrTy, "sys_rl_buf_addr");
                        llvm::Value* endOffset = builder.CreateTrunc(sysRawResult, builder.getInt32Ty(), "sys_rl_off32");
                        (void)endOffset;
                        llvm::Value* endAddr = builder.CreateGEP(i8Ty, bufAddr, {sysRawResult}, "sys_rl_end");
                        builder.CreateStore(builder.getInt8(0), endAddr);
                    }
                    
                    // Convert buffer to Aria string via strdup (returns char*, FFI wraps to AriaString)
                    // Declare strdup: char* strdup(const char*)
                    llvm::FunctionCallee strdupFn = module->getOrInsertFunction("strdup",
                        llvm::FunctionType::get(ptrTy, {ptrTy}, false));
                    llvm::Value* bufAddrPtr = builder.CreateIntToPtr(bufPtr, ptrTy, "sys_strbuf_addr");
                    llvm::Value* dupPtr = builder.CreateCall(strdupFn, {bufAddrPtr}, "sys_strbuf_dup");
                    
                    // Get string length via strlen
                    llvm::FunctionCallee strlenFn = module->getOrInsertFunction("strlen",
                        llvm::FunctionType::get(i64Ty, {ptrTy}, false));
                    llvm::Value* strLen = builder.CreateCall(strlenFn, {dupPtr}, "sys_strbuf_len");
                    
                    // GC-allocate AriaString struct and copy data to GC memory
                    llvm::Value* dataSz = builder.CreateAdd(strLen, builder.getInt64(1), "sys_strbuf_dsz");
                    llvm::Value* gcData = builder.CreateCall(gcAlloc, {dataSz}, "sys_strbuf_gcdata");
                    llvm::FunctionCallee memcpyFn = module->getOrInsertFunction("memcpy",
                        llvm::FunctionType::get(ptrTy, {ptrTy, ptrTy, i64Ty}, false));
                    builder.CreateCall(memcpyFn, {gcData, dupPtr, dataSz});
                    
                    // Free strdup allocation
                    llvm::FunctionCallee freeFn = module->getOrInsertFunction("free",
                        llvm::FunctionType::get(llvm::Type::getVoidTy(context), {ptrTy}, false));
                    builder.CreateCall(freeFn, {dupPtr});
                    
                    // Build AriaString struct
                    llvm::Value* ariaStr = llvm::UndefValue::get(ariaStrType);
                    ariaStr = builder.CreateInsertValue(ariaStr, gcData, 0, "sys_strbuf_str.data");
                    ariaStr = builder.CreateInsertValue(ariaStr, strLen, 1, "sys_strbuf_str.len");
                    llvm::Value* ariaStrPtr = builder.CreateCall(gcAlloc, {builder.getInt64(16)}, "sys_strbuf_str_ptr");
                    builder.CreateStore(ariaStr, ariaStrPtr);
                    
                    // munmap the buffer now
                    std::vector<llvm::Type*> munmapParamTypes(3, i64Ty);
                    llvm::FunctionType* munmapAsmType = llvm::FunctionType::get(i64Ty, munmapParamTypes, false);
                    llvm::InlineAsm* munmapAsm = llvm::InlineAsm::get(
                        munmapAsmType, "syscall",
                        "={rax},{rax},{rdi},{rsi},~{rcx},~{r11},~{memory}",
                        true, false, llvm::InlineAsm::AD_ATT);
                    builder.CreateCall(munmapAsmType, munmapAsm, {
                        builder.getInt64(11), bufPtr, bufSize
                    }, "sys_strbuf_munmap_ok");
                    
                    // Build success Result<string>
                    llvm::Value* okResult = llvm::UndefValue::get(resultStrTy);
                    okResult = builder.CreateInsertValue(okResult, ariaStrPtr, 0, "sys_strbuf_ok.val");
                    okResult = builder.CreateInsertValue(okResult, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptrTy)), 1, "sys_strbuf_ok.err");
                    okResult = builder.CreateInsertValue(okResult, llvm::ConstantInt::get(i8Ty, 0), 2, "sys_strbuf_ok.flag");
                    builder.CreateStore(okResult, resultAlloca);
                    builder.CreateBr(mergeBB);
                }
                
                // --- Merge block: load Result<string> from alloca ---
                builder.SetInsertPoint(mergeBB);
                llvm::Value* finalResult = builder.CreateLoad(resultStrTy, resultAlloca, "sys_strbuf_final");
                return finalResult;
            }
        }
        
        // --- Evaluate and convert syscall arguments to i64 ---
        std::vector<llvm::Value*> syscallArgVals;
        for (size_t i = 1; i < numArgs; i++) {
            llvm::Value* argVal = codegenExpressionNode(expr->arguments[i].get(), this);
            if (!argVal) {
                throw std::runtime_error("Failed to generate syscall argument " + std::to_string(i));
            }
            
            // Convert to i64 for register passing
            if (argVal->getType()->isIntegerTy(64)) {
                // Already i64
            } else if (argVal->getType()->isIntegerTy()) {
                argVal = builder.CreateZExt(argVal, builder.getInt64Ty(), "sys_arg_zext");
            } else if (llvm::isa<llvm::GlobalVariable>(argVal)) {
                // String literal: GlobalVariable pointing to struct.AriaString
                // Load the struct and extract the char* data field
                llvm::GlobalVariable* gv = llvm::cast<llvm::GlobalVariable>(argVal);
                llvm::Type* gvType = gv->getValueType();
                if (gvType->isStructTy()) {
                    llvm::Value* strStruct = builder.CreateLoad(
                        gvType, argVal, "sys_str_struct");
                    llvm::Value* dataPtr = builder.CreateExtractValue(
                        strStruct, 0, "sys_str_data");
                    argVal = builder.CreatePtrToInt(dataPtr, builder.getInt64Ty(), "sys_str_int");
                } else {
                    argVal = builder.CreatePtrToInt(argVal, builder.getInt64Ty(), "sys_gv_int");
                }
            } else if (argVal->getType()->isPointerTy()) {
                // Could be AriaString* (string variable) or raw pointer
                // Try to load as AriaString struct and extract data
                llvm::StructType* ariaStringType = llvm::StructType::getTypeByName(context, "struct.AriaString");
                if (ariaStringType) {
                    // Check if this looks like a string variable by trying AriaString load
                    // For non-string pointers (wild T@), just pass the pointer as-is
                    // Heuristic: if the type checker said this arg is a string, treat as AriaString
                    llvm::Value* strStruct = builder.CreateLoad(ariaStringType, argVal, "sys_str_struct");
                    llvm::Value* dataPtr = builder.CreateExtractValue(strStruct, 0, "sys_str_data");
                    argVal = builder.CreatePtrToInt(dataPtr, builder.getInt64Ty(), "sys_strvar_int");
                } else {
                    // Raw pointer — just convert to int
                    argVal = builder.CreatePtrToInt(argVal, builder.getInt64Ty(), "sys_ptr_int");
                }
            } else if (argVal->getType()->isStructTy()) {
                // Struct value (e.g., AriaString by value) — extract element 0
                argVal = builder.CreateExtractValue(argVal, {0}, "sys_struct_ptr");
                if (argVal->getType()->isPointerTy()) {
                    argVal = builder.CreatePtrToInt(argVal, builder.getInt64Ty(), "sys_struct_int");
                } else if (!argVal->getType()->isIntegerTy(64)) {
                    argVal = builder.CreateZExtOrTrunc(argVal, builder.getInt64Ty(), "sys_struct_cast");
                }
            } else {
                throw std::runtime_error("sys() argument " + std::to_string(i + 1) +
                                         ": unsupported type for syscall register");
            }
            syscallArgVals.push_back(argVal);
        }
        
        // --- Build inline asm for the syscall instruction ---
        // x86-64 syscall ABI:
        //   rax = syscall number
        //   rdi = arg1, rsi = arg2, rdx = arg3, r10 = arg4, r8 = arg5, r9 = arg6
        //   return in rax
        //   rcx and r11 are clobbered by kernel
        
        // Build function type: i64(i64 nr, i64 a1, i64 a2, ...) 
        std::vector<llvm::Type*> asmParamTypes(1 + numSyscallArgs, builder.getInt64Ty());
        llvm::FunctionType* asmFuncType = llvm::FunctionType::get(
            builder.getInt64Ty(), asmParamTypes, false);
        
        // Build constraint string
        std::string constraints = "={rax},{rax}";
        for (size_t i = 0; i < numSyscallArgs; i++) {
            constraints += ",";
            constraints += regConstraints[i];
        }
        constraints += ",~{rcx},~{r11},~{memory}";
        
        llvm::InlineAsm* sysAsm = llvm::InlineAsm::get(
            asmFuncType,
            "syscall",
            constraints,
            /*hasSideEffects=*/true,
            /*isAlignStack=*/false,
            llvm::InlineAsm::AD_ATT
        );
        
        // Build call arguments: [syscall_nr, arg1, arg2, ...]
        std::vector<llvm::Value*> asmArgs;
        asmArgs.push_back(syscallNr);
        asmArgs.insert(asmArgs.end(), syscallArgVals.begin(), syscallArgVals.end());
        
        llvm::Value* rawResult = builder.CreateCall(asmFuncType, sysAsm, asmArgs, "syscall_ret");
        
        // --- Wrap return value ---
        if (isRaw) {
            // sys!!!() → return bare int64
            return rawResult;
        }
        
        // sys() and sys!!() → wrap in Result<int64>
        // Result<int64> = { int64 value, ptr error, i8 is_error }
        // If rawResult >= 0: pass (value = rawResult, error = null, is_error = 0)
        // If rawResult < 0:  fail (value = 0, error = inttoptr(-rawResult), is_error = 1)
        
        llvm::Type* i64Ty = builder.getInt64Ty();
        llvm::Type* ptrTy = llvm::PointerType::get(context, 0);
        llvm::Type* i8Ty = builder.getInt8Ty();
        
        llvm::StructType* resultTy = llvm::StructType::get(context, {i64Ty, ptrTy, i8Ty});
        
        // Check: rawResult < 0 (indicates error, -errno)
        llvm::Value* isNeg = builder.CreateICmpSLT(rawResult, builder.getInt64(0), "sys_is_err");
        
        // Compute negated value for error code
        llvm::Value* negResult = builder.CreateNeg(rawResult, "sys_neg_errno");
        llvm::Value* errPtr = builder.CreateIntToPtr(negResult, ptrTy, "sys_err_ptr");
        llvm::Value* nullPtr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptrTy));
        
        // Select values based on error condition
        llvm::Value* val = builder.CreateSelect(isNeg, builder.getInt64(0), rawResult, "sys_val");
        llvm::Value* err = builder.CreateSelect(isNeg, errPtr, nullPtr, "sys_err");
        llvm::Value* flag = builder.CreateSelect(isNeg,
            llvm::ConstantInt::get(i8Ty, 1),
            llvm::ConstantInt::get(i8Ty, 0),
            "sys_flag");
        
        // Build the Result struct
        llvm::Value* result = llvm::UndefValue::get(resultTy);
        result = builder.CreateInsertValue(result, val, 0, "sys_result.val");
        result = builder.CreateInsertValue(result, err, 1, "sys_result.err");
        result = builder.CreateInsertValue(result, flag, 2, "sys_result.is_error");
        
        return result;
    }
    
    // Helper to get or declare aria_string_from_cstr_simple
    auto getOrDeclareStringFromCstr = [&]() -> llvm::Function* {
        llvm::Function* func = module->getFunction("aria_string_from_cstr_simple");
        if (!func) {
            llvm::StructType* strType = llvm::StructType::getTypeByName(context, "struct.AriaString");
            if (!strType) {
                strType = llvm::StructType::create(context,
                    {llvm::PointerType::get(builder.getInt8Ty(), 0), builder.getInt64Ty()},
                    "struct.AriaString");
            }
            std::vector<llvm::Type*> params = {llvm::PointerType::get(builder.getInt8Ty(), 0)};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(strType, 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_from_cstr_simple", module);
        }
        return func;
    };

    // stdin_read_line() -> string
    if (callee_ident->name == "stdin_read_line") {
        if (expr->arguments.size() != 0) {
            throw std::runtime_error("stdin_read_line() takes no arguments");
        }
        
        llvm::Function* read_func = module->getFunction("aria_stdin_read_line");
        if (!read_func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(context, 0),  // returns char*
                {},  // no arguments
                false
            );
            read_func = llvm::Function::Create(
                func_type,
                llvm::Function::ExternalLinkage,
                "aria_stdin_read_line",
                module
            );
        }
        
        llvm::Value* cstr = builder.CreateCall(read_func, {}, "stdin_read_call");
        return builder.CreateCall(getOrDeclareStringFromCstr(), {cstr}, "stdin_line_str");
    }

    // stdin_read_all() -> string
    if (callee_ident->name == "stdin_read_all") {
        if (expr->arguments.size() != 0) {
            throw std::runtime_error("stdin_read_all() takes no arguments");
        }
        
        llvm::Function* read_func = module->getFunction("aria_stdin_read_all");
        if (!read_func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(context, 0),
                {},
                false
            );
            read_func = llvm::Function::Create(
                func_type,
                llvm::Function::ExternalLinkage,
                "aria_stdin_read_all",
                module
            );
        }
        
        llvm::Value* cstr = builder.CreateCall(read_func, {}, "stdin_readall_call");
        return builder.CreateCall(getOrDeclareStringFromCstr(), {cstr}, "stdin_all_str");
    }
    
    // ====================================================================
    // FRAC (EXACT RATIONAL) ARITHMETIC INTRINSICS
    // ====================================================================
    // Mixed-number fractions: frac32 = {tbb32 whole, tbb32 num, tbb32 denom}
    // All operations call C runtime for exact rational arithmetic with GCD reduction
    
    // Helper lambda to get frac LLVM type  
    auto get_frac_type = [&](const std::string& frac_name) -> llvm::StructType* {
        int bits = 0;
        if (frac_name == "frac8") bits = 8;
        else if (frac_name == "frac16") bits = 16;
        else if (frac_name == "frac32") bits = 32;
        else if (frac_name == "frac64") bits = 64;
        else throw std::runtime_error("Invalid frac type: " + frac_name);
        
        llvm::Type* tbb_type = llvm::Type::getIntNTy(context, bits);
        return llvm::StructType::get(context, {tbb_type, tbb_type, tbb_type});
    };
    
    // frac*_from_parts(whole, num, denom) -> frac*
    for (const char* width : {"8", "16", "32", "64"}) {
        std::string frac_name = "frac" + std::string(width);
        std::string func_name = frac_name + "_from_parts";
        
        if (callee_ident->name == func_name || callee_ident->name == "@" + func_name) {
            if (expr->arguments.size() != 3) {
                throw std::runtime_error(func_name + "() requires exactly 3 arguments");
            }
            
            // Get arguments
            llvm::Value* whole = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* num = codegenExpressionNode(expr->arguments[1].get(), this);
            llvm::Value* denom = codegenExpressionNode(expr->arguments[2].get(), this);
            
            int bits = std::stoi(width);
            llvm::Type* tbb_type = llvm::Type::getIntNTy(context, bits);
            
            // Convert arguments to tbb type
            whole = builder.CreateIntCast(whole, tbb_type, true, "whole.cast");
            num = builder.CreateIntCast(num, tbb_type, true, "num.cast");
            denom = builder.CreateIntCast(denom, tbb_type, true, "denom.cast");
            
            // Create frac struct inline (no runtime call needed for constructor)
            llvm::StructType* frac_type = get_frac_type(frac_name);
            llvm::Value* result = llvm::UndefValue::get(frac_type);
            result = builder.CreateInsertValue(result, whole, 0, "frac.whole");
            result = builder.CreateInsertValue(result, num, 1, "frac.num");
            result = builder.CreateInsertValue(result, denom, 2, "frac.denom");
            
            return result;
        }
    }
    
    // frac*_add/sub/mul/div(a, b) -> frac*
    for (const char* width : {"8", "16", "32", "64"}) {
        std::string frac_name = "frac" + std::string(width);
        std::string struct_name = "Frac" + std::string(width);
        
        for (const char* op : {"add", "sub", "mul", "div"}) {
            std::string func_name = frac_name + "_" + op;
            std::string runtime_func = "aria_" + func_name;
            
            if (callee_ident->name == func_name || callee_ident->name == "@" + func_name) {
                if (expr->arguments.size() != 2) {
                    throw std::runtime_error(func_name + "() requires exactly 2 arguments");
                }
                
                // Get or create C runtime function
                llvm::StructType* frac_type = get_frac_type(frac_name);
                llvm::Function* c_func = module->getFunction(runtime_func);
                if (!c_func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        builder.getVoidTy(),  // void return (sret style)
                        {llvm::PointerType::get(frac_type, 0),  // result*
                         llvm::PointerType::get(frac_type, 0),  // a*
                         llvm::PointerType::get(frac_type, 0)}, // b*
                        false
                    );
                    c_func = llvm::Function::Create(func_type,
                        llvm::Function::ExternalLinkage, runtime_func, module);
                }
                
                // Get arguments
                llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
                llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
                
                // Create alloc as for passing to C runtime
                llvm::Value* result_alloca = builder.CreateAlloca(frac_type, nullptr, "result");
                llvm::Value* a_alloca = builder.CreateAlloca(frac_type, nullptr, "a.tmp");
                llvm::Value* b_alloca = builder.CreateAlloca(frac_type, nullptr, "b.tmp");
                
                // Store arguments
                builder.CreateStore(a, a_alloca);
                builder.CreateStore(b, b_alloca);
                
                // Call C runtime
                builder.CreateCall(c_func, {result_alloca, a_alloca, b_alloca});
                
                // Load and return result
                return builder.CreateLoad(frac_type, result_alloca, "frac.result");
            }
        }
    }
    
    // frac*_neg(a) -> frac*
    for (const char* width : {"8", "16", "32", "64"}) {
        std::string frac_name = "frac" + std::string(width);
        std::string func_name = frac_name + "_neg";
        std::string runtime_func = "aria_" + func_name;
        
        if (callee_ident->name == func_name || callee_ident->name == "@" + func_name) {
            if (expr->arguments.size() != 1) {
                throw std::runtime_error(func_name + "() requires exactly 1 argument");
            }
            
            llvm::StructType* frac_type = get_frac_type(frac_name);
            llvm::Function* c_func = module->getFunction(runtime_func);
            if (!c_func) {
                llvm::FunctionType* func_type = llvm::FunctionType::get(
                    builder.getVoidTy(),
                    {llvm::PointerType::get(frac_type, 0),  // result*
                     llvm::PointerType::get(frac_type, 0)}, // a*
                    false
                );
                c_func = llvm::Function::Create(func_type,
                    llvm::Function::ExternalLinkage, runtime_func, module);
            }
            
            llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* result_alloca = builder.CreateAlloca(frac_type, nullptr, "result");
            llvm::Value* a_alloca = builder.CreateAlloca(frac_type, nullptr, "a.tmp");
            builder.CreateStore(a, a_alloca);
            builder.CreateCall(c_func, {result_alloca, a_alloca});
            return builder.CreateLoad(frac_type, result_alloca, "frac.neg");
        }
    }
    
    // frac*_cmp(a, b) -> int32
    for (const char* width : {"8", "16", "32", "64"}) {
        std::string frac_name = "frac" + std::string(width);
        std::string func_name = frac_name + "_cmp";
        std::string runtime_func = "aria_" + func_name;
        
        if (callee_ident->name == func_name || callee_ident->name == "@" + func_name) {
            if (expr->arguments.size() != 2) {
                throw std::runtime_error(func_name + "() requires exactly 2 arguments");
            }
            
            llvm::StructType* frac_type = get_frac_type(frac_name);
            llvm::Function* c_func = module->getFunction(runtime_func);
            if (!c_func) {
                llvm::FunctionType* func_type = llvm::FunctionType::get(
                    builder.getInt32Ty(),
                    {llvm::PointerType::get(frac_type, 0),  // a*
                     llvm::PointerType::get(frac_type, 0)}, // b*
                    false
                );
                c_func = llvm::Function::Create(func_type,
                    llvm::Function::ExternalLinkage, runtime_func, module);
            }
            
            llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
            llvm::Value* a_alloca = builder.CreateAlloca(frac_type, nullptr, "a.tmp");
            llvm::Value* b_alloca = builder.CreateAlloca(frac_type, nullptr, "b.tmp");
            builder.CreateStore(a, a_alloca);
            builder.CreateStore(b, b_alloca);
            return builder.CreateCall(c_func, {a_alloca, b_alloca}, "frac.cmp");
        }
    }
    
    // frac*_to_int(a) -> int32/int64
    for (const char* width : {"8", "16", "32", "64"}) {
        std::string frac_name = "frac" + std::string(width);
        std::string func_name = frac_name + "_to_int";
        std::string runtime_func = "aria_" + func_name;
        
        if (callee_ident->name == func_name || callee_ident->name == "@" + func_name) {
            if (expr->arguments.size() != 1) {
                throw std::runtime_error(func_name + "() requires exactly 1 argument");
            }
            
            llvm::Type* ret_type = (std::string(width) == "64") ? 
                builder.getInt64Ty() : builder.getInt32Ty();
            
            llvm::StructType* frac_type = get_frac_type(frac_name);
            llvm::Function* c_func = module->getFunction(runtime_func);
            if (!c_func) {
                llvm::FunctionType* func_type = llvm::FunctionType::get(
                    ret_type,
                    {llvm::PointerType::get(frac_type, 0)},  // a*
                    false
                );
                c_func = llvm::Function::Create(func_type,
                    llvm::Function::ExternalLinkage, runtime_func, module);
            }
            
            llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* a_alloca = builder.CreateAlloca(frac_type, nullptr, "a.tmp");
            builder.CreateStore(a, a_alloca);
            return builder.CreateCall(c_func, {a_alloca}, "frac.to_int");
        }
    }
    
    // frac*_to_float(a) -> flt32/flt64
    for (const char* width : {"8", "16", "32", "64"}) {
        std::string frac_name = "frac" + std::string(width);
        std::string func_name = frac_name + "_to_float";
        std::string runtime_func = "aria_" + func_name;
        
        if (callee_ident->name == func_name || callee_ident->name == "@" + func_name) {
            if (expr->arguments.size() != 1) {
                throw std::runtime_error(func_name + "() requires exactly 1 argument");
            }
            
            llvm::Type* ret_type = (std::string(width) == "64") ? 
                builder.getDoubleTy() : builder.getFloatTy();
            
            llvm::StructType* frac_type = get_frac_type(frac_name);
            llvm::Function* c_func = module->getFunction(runtime_func);
            if (!c_func) {
                llvm::FunctionType* func_type = llvm::FunctionType::get(
                    ret_type,
                    {llvm::PointerType::get(frac_type, 0)},  // a*
                    false
                );
                c_func = llvm::Function::Create(func_type,
                    llvm::Function::ExternalLinkage, runtime_func, module);
            }
            
            llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* a_alloca = builder.CreateAlloca(frac_type, nullptr, "a.tmp");
            builder.CreateStore(a, a_alloca);
            return builder.CreateCall(c_func, {a_alloca}, "frac.to_float");
        }
    }
    
    // ====================================================================
    // ATOMIC<T> OPERATIONS - Lock-free Concurrency Primitives
    // ====================================================================
    // P0 Feature: atomic<T> type and operations for thread-safe data structures
    // Runtime: C++11 std::atomic wrappers with TBB sticky error propagation
    // Memory Ordering: SeqCst (sequentially consistent) for safety - relaxed orderings future work
    //
    // Supported Types: int8-64, uint8-64, bool, tbb8-64 (with CAS-based ERR propagation)
    // Operations: load, store, swap, compare_exchange, fetch_add, fetch_sub
    //
    // Design: atomic_new(initial) creates atomic, methods dispatch to runtime:
    //   counter.load() → aria_atomic_TYPE_load(&counter, SEQCST)
    //   counter.store(val) → aria_atomic_TYPE_store(&counter, val, SEQCST)
    //   etc.
    
    // atomic_new(initial_value) -> atomic<T>*
    // Creates a new atomic with the given initial value
    // Type deduction: infer T from argument type (e.g., 0i32 → atomic<int32>)
    if (callee_ident->name == "atomic_new") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("atomic_new() requires exactly one argument");
        }
        
        // Evaluate the initial value argument
        llvm::Value* initial_val = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!initial_val) {
            throw std::runtime_error("Failed to generate code for atomic_new() argument");
        }
        
        // Determine the type from the argument
        llvm::Type* arg_type = initial_val->getType();
        std::string runtime_type_name;
        std::string aria_type_name;
        
        // Map LLVM type to runtime type name
        if (arg_type->isIntegerTy(8)) {
            runtime_type_name = "int8";
            aria_type_name = "int8";
        } else if (arg_type->isIntegerTy(16)) {
            runtime_type_name = "int16";
            aria_type_name = "int16";
        } else if (arg_type->isIntegerTy(32)) {
            runtime_type_name = "int32";
            aria_type_name = "int32";
        } else if (arg_type->isIntegerTy(64)) {
            runtime_type_name = "int64";
            aria_type_name = "int64";
        } else if (arg_type->isIntegerTy(1)) {
            runtime_type_name = "bool";
            aria_type_name = "bool";
        } else {
            throw std::runtime_error("atomic_new() currently supports int8-64 and bool types");
        }
        
        // Construct runtime function name: aria_atomic_TYPE_create
        std::string runtime_func_name = "aria_atomic_" + runtime_type_name + "_create";
        
        // Get or declare the runtime function
        llvm::Function* create_func = module->getFunction(runtime_func_name);
        if (!create_func) {
            // Get the opaque atomic type
            std::string atomic_struct_name = "struct.AriaAtomic";
            for (char& c : runtime_type_name) {
                if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';  // toupper
            }
            atomic_struct_name += runtime_type_name.substr(0, 1);
            atomic_struct_name += runtime_type_name.substr(1);
            
            // Get proper capitalization: int32 → Int32
            std::string capitalized = runtime_type_name;
            if (!capitalized.empty()) {
                capitalized[0] = std::toupper(capitalized[0]);
            }
            atomic_struct_name = "struct.AriaAtomic" + capitalized;
            
            llvm::StructType* atomic_type = llvm::StructType::create(context, atomic_struct_name);
            
            // Signature: AriaAtomicTYPE* aria_atomic_TYPE_create(TYPE initial)
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(atomic_type, 0),  // returns atomic ptr
                {arg_type},                              // takes initial value
                false
            );
            
            create_func = llvm::Function::Create(
                func_type,
                llvm::Function::ExternalLinkage,
                runtime_func_name,
                module
            );
        }
        
        // Call the runtime function
        return builder.CreateCall(create_func, {initial_val}, "atomic_new");
    }
    
    // ====================================================================
    // TFP (TWISTED FLOATING POINT) INTRINSICS
    // ====================================================================
    // Deterministic cross-platform floating-point
    // tfp32: {i16 exp, i16 mant}
    // tfp64: {i16 exp, i48 mant, i16 _pad}
    
    //tfp*_from_parts(exp, mant) -> tfp*
    for (const char* width : {"32", "64"}) {
        std::string funcName = "tfp" + std::string(width) + "_from_parts";
        if (callee_ident->name == funcName || callee_ident->name == "@" + funcName) {
            if (expr->arguments.size() != 2) {
                throw std::runtime_error(funcName + "() requires exactly 2 arguments");
            }
            
            llvm::Value* exp_val = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* mant_val = codegenExpressionNode(expr->arguments[1].get(), this);
            
            // Build tfp struct inline
            llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
            llvm::Value* tfp_val = llvm::UndefValue::get(tfp_type);
            tfp_val = builder.CreateInsertValue(tfp_val, exp_val, 0, "tfp.exp");
            tfp_val = builder.CreateInsertValue(tfp_val, mant_val, 1, "tfp.mant");
            return tfp_val;
        }
    }
    
    // tfp*_from_double(double) -> tfp*
    for (const char* width : {"32", "64"}) {
        std::string funcName = "aria_tfp" + std::string(width) + "_from_double";
        std::string ariaName = "tfp" + std::string(width) + "_from_double";
        if (callee_ident->name == ariaName || callee_ident->name == "@" + ariaName) {
            if (expr->arguments.size() != 1) {
                throw std::runtime_error(ariaName + "() requires exactly 1 argument");
            }
            
            llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
            llvm::FunctionType* c_func_type = llvm::FunctionType::get(tfp_type, {builder.getDoubleTy()}, false);
            llvm::Function* c_func = module->getFunction(funcName);
            if (!c_func) {
                c_func = llvm::Function::Create(c_func_type, llvm::Function::ExternalLinkage, funcName, module);
            }
            
            llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
            return builder.CreateCall(c_func, {arg}, "tfp.from_double");
        }
    }
    
    // tfp*_to_double(tfp*) -> flt64
    for (const char* width : {"32", "64"}) {
        std::string funcName = "aria_tfp" + std::string(width) + "_to_double";
        std::string ariaName = "tfp" + std::string(width) + "_to_double";
        if (callee_ident->name == ariaName || callee_ident->name == "@" + ariaName) {
            if (expr->arguments.size() != 1) {
                throw std::runtime_error(ariaName + "() requires exactly 1 argument");
            }
            
            llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
            llvm::FunctionType* c_func_type = llvm::FunctionType::get(builder.getDoubleTy(), {tfp_type}, false);
            llvm::Function* c_func = module->getFunction(funcName);
            if (!c_func) {
                c_func = llvm::Function::Create(c_func_type, llvm::Function::ExternalLinkage, funcName, module);
            }
            
            llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
            return builder.CreateCall(c_func, {arg}, "tfp.to_double");
        }
    }
    
    // tfp* arithmetic: add, sub, mul, div
    for (const char* width : {"32", "64"}) {
        for (const char* op : {"add", "sub", "mul", "div"}) {
            std::string funcName = "aria_tfp" + std::string(width) + "_" + op;
            std::string ariaName = "tfp" + std::string(width) + "_" + op;
            if (callee_ident->name == ariaName || callee_ident->name == "@" + ariaName) {
                if (expr->arguments.size() != 2) {
                    throw std::runtime_error(ariaName + "() requires exactly 2 arguments");
                }
                
                llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
                llvm::FunctionType* c_func_type = llvm::FunctionType::get(tfp_type, {tfp_type, tfp_type}, false);
                llvm::Function* c_func = module->getFunction(funcName);
                if (!c_func) {
                    c_func = llvm::Function::Create(c_func_type, llvm::Function::ExternalLinkage, funcName, module);
                }
                
                llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
                llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
                return builder.CreateCall(c_func, {a, b}, "tfp." + std::string(op));
            }
        }
    }
    
    // tfp*_neg(tfp*) -> tfp*
    for (const char* width : {"32", "64"}) {
        std::string funcName = "aria_tfp" + std::string(width) + "_neg";
        std::string ariaName = "tfp" + std::string(width) + "_neg";
        if (callee_ident->name == ariaName || callee_ident->name == "@" + ariaName) {
            if (expr->arguments.size() != 1) {
                throw std::runtime_error(ariaName + "() requires exactly 1 argument");
            }
            
            llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
            llvm::FunctionType* c_func_type = llvm::FunctionType::get(tfp_type, {tfp_type}, false);
            llvm::Function* c_func = module->getFunction(funcName);
            if (!c_func) {
                c_func = llvm::Function::Create(c_func_type, llvm::Function::ExternalLinkage, funcName, module);
            }
            
            llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
            return builder.CreateCall(c_func, {a}, "tfp.neg");
        }
    }
    
    // tfp*_cmp(tfp*, tfp*) -> int32
    for (const char* width : {"32", "64"}) {
        std::string funcName = "aria_tfp" + std::string(width) + "_cmp";
        std::string ariaName = "tfp" + std::string(width) + "_cmp";
        if (callee_ident->name == ariaName || callee_ident->name == "@" + ariaName) {
            if (expr->arguments.size() != 2) {
                throw std::runtime_error(ariaName + "() requires exactly 2 arguments");
            }
            
            llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
            llvm::FunctionType* c_func_type = llvm::FunctionType::get(builder.getInt32Ty(), {tfp_type, tfp_type}, false);
            llvm::Function* c_func = module->getFunction(funcName);
            if (!c_func) {
                c_func = llvm::Function::Create(c_func_type, llvm::Function::ExternalLinkage, funcName, module);
            }
            
            llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
            return builder.CreateCall(c_func, {a, b}, "tfp.cmp");
        }
    }
    
    // tfp* math functions: sqrt, sin, cos, exp, log (single arg)
    for (const char* width : {"32", "64"}) {
        for (const char* mathfunc : {"sqrt", "sin", "cos", "exp", "log"}) {
            std::string funcName = "aria_tfp" + std::string(width) + "_" + mathfunc;
            std::string ariaName = "tfp" + std::string(width) + "_" + mathfunc;
            if (callee_ident->name == ariaName || callee_ident->name == "@" + ariaName) {
                if (expr->arguments.size() != 1) {
                    throw std::runtime_error(ariaName + "() requires exactly 1 argument");
                }
                
                llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
                llvm::FunctionType* c_func_type = llvm::FunctionType::get(tfp_type, {tfp_type}, false);
                llvm::Function* c_func = module->getFunction(funcName);
                if (!c_func) {
                    c_func = llvm::Function::Create(c_func_type, llvm::Function::ExternalLinkage, funcName, module);
                }
                
                llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
                return builder.CreateCall(c_func, {a}, std::string("tfp.") + mathfunc);
            }
        }
    }
    
    // tfp*_pow(base, exp) -> tfp*
    for (const char* width : {"32", "64"}) {
        std::string funcName = "aria_tfp" + std::string(width) + "_pow";
        std::string ariaName = "tfp" + std::string(width) + "_pow";
        if (callee_ident->name == ariaName || callee_ident->name == "@" + ariaName) {
            if (expr->arguments.size() != 2) {
                throw std::runtime_error(ariaName + "() requires exactly 2 arguments");
            }
            
            llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
            llvm::FunctionType* c_func_type = llvm::FunctionType::get(tfp_type, {tfp_type, tfp_type}, false);
            llvm::Function* c_func = module->getFunction(funcName);
            if (!c_func) {
                c_func = llvm::Function::Create(c_func_type, llvm::Function::ExternalLinkage, funcName, module);
            }
            
            llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
            return builder.CreateCall(c_func, {a, b}, "tfp.pow");
        }
    }
    
    // ====================================================================
    // ARENA ALLOCATOR BUILTINS (Phase 4.2.5.2)
    // ====================================================================
    
    if (callee_ident->name == "arena_new") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("arena_new() requires exactly one argument");
        }
        
        llvm::Function* arena_new_func = module->getFunction("aria_arena_new_handle");
        if (!arena_new_func) {
            llvm::FunctionType* arena_new_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            arena_new_func = llvm::Function::Create(arena_new_type,
                llvm::Function::ExternalLinkage, "aria_arena_new_handle", module);
        }
        
        llvm::Value* capacity = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!capacity->getType()->isIntegerTy(64)) {
            capacity = builder.CreateSExtOrTrunc(capacity, builder.getInt64Ty());
        }
        return builder.CreateCall(arena_new_func, {capacity}, "arena_handle");
    }
    
    if (callee_ident->name == "arena_alloc") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("arena_alloc() requires two arguments");
        }
        
        llvm::Function* arena_alloc_func = module->getFunction("aria_arena_alloc_handle");
        if (!arena_alloc_func) {
            llvm::FunctionType* arena_alloc_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty(), builder.getInt64Ty()}, false);
            arena_alloc_func = llvm::Function::Create(arena_alloc_type,
                llvm::Function::ExternalLinkage, "aria_arena_alloc_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* size = codegenExpressionNode(expr->arguments[1].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        if (!size->getType()->isIntegerTy(64)) {
            size = builder.CreateSExtOrTrunc(size, builder.getInt64Ty());
        }
        return builder.CreateCall(arena_alloc_func, {handle, size}, "alloc_ptr");
    }
    
    if (callee_ident->name == "arena_reset") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("arena_reset() requires one argument");
        }
        
        llvm::Function* arena_reset_func = module->getFunction("aria_arena_reset_handle");
        if (!arena_reset_func) {
            llvm::FunctionType* arena_reset_type = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt64Ty()}, false);
            arena_reset_func = llvm::Function::Create(arena_reset_type,
                llvm::Function::ExternalLinkage, "aria_arena_reset_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        builder.CreateCall(arena_reset_func, {handle});
        return builder.getInt32(0);
    }
    
    if (callee_ident->name == "arena_destroy") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("arena_destroy() requires one argument");
        }
        
        llvm::Function* arena_destroy_func = module->getFunction("aria_arena_destroy_handle");
        if (!arena_destroy_func) {
            llvm::FunctionType* arena_destroy_type = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt64Ty()}, false);
            arena_destroy_func = llvm::Function::Create(arena_destroy_type,
                llvm::Function::ExternalLinkage, "aria_arena_destroy_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        builder.CreateCall(arena_destroy_func, {handle});
        return builder.getInt32(0);
    }
    
    if (callee_ident->name == "arena_get_allocated") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("arena_get_allocated() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_arena_get_allocated_handle");
        if (!func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_arena_get_allocated_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(func, {handle}, "allocated_bytes");
    }
    
    if (callee_ident->name == "arena_get_reserved") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("arena_get_reserved() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_arena_get_reserved_handle");
        if (!func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_arena_get_reserved_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(func, {handle}, "reserved_bytes");
    }
    
    // ====================================================================
    // END ARENA BUILTINS
    // ====================================================================
    
    // ====================================================================
    // POOL ALLOCATOR BUILTINS (Phase 4.2.5.3)
    // ====================================================================
    
    if (callee_ident->name == "pool_new") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("pool_new() requires exactly two arguments");
        }
        
        llvm::Function* pool_new_func = module->getFunction("aria_pool_new_handle");
        if (!pool_new_func) {
            llvm::FunctionType* pool_new_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty(), builder.getInt64Ty()}, false);
            pool_new_func = llvm::Function::Create(pool_new_type,
                llvm::Function::ExternalLinkage, "aria_pool_new_handle", module);
        }
        
        llvm::Value* block_size = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* capacity = codegenExpressionNode(expr->arguments[1].get(), this);
        if (!block_size->getType()->isIntegerTy(64)) {
            block_size = builder.CreateSExtOrTrunc(block_size, builder.getInt64Ty());
        }
        if (!capacity->getType()->isIntegerTy(64)) {
            capacity = builder.CreateSExtOrTrunc(capacity, builder.getInt64Ty());
        }
        return builder.CreateCall(pool_new_func, {block_size, capacity}, "pool_handle");
    }
    
    if (callee_ident->name == "pool_alloc") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("pool_alloc() requires one argument");
        }
        
        llvm::Function* pool_alloc_func = module->getFunction("aria_pool_alloc_handle");
        if (!pool_alloc_func) {
            llvm::FunctionType* pool_alloc_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            pool_alloc_func = llvm::Function::Create(pool_alloc_type,
                llvm::Function::ExternalLinkage, "aria_pool_alloc_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(pool_alloc_func, {handle}, "alloc_ptr");
    }
    
    if (callee_ident->name == "pool_free") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("pool_free() requires two arguments");
        }
        
        llvm::Function* pool_free_func = module->getFunction("aria_pool_free_handle");
        if (!pool_free_func) {
            llvm::FunctionType* pool_free_type = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt64Ty(), builder.getInt64Ty()}, false);
            pool_free_func = llvm::Function::Create(pool_free_type,
                llvm::Function::ExternalLinkage, "aria_pool_free_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        if (!ptr->getType()->isIntegerTy(64)) {
            ptr = builder.CreateSExtOrTrunc(ptr, builder.getInt64Ty());
        }
        builder.CreateCall(pool_free_func, {handle, ptr});
        return builder.getInt32(0);
    }
    
    if (callee_ident->name == "pool_reset") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("pool_reset() requires one argument");
        }
        
        llvm::Function* pool_reset_func = module->getFunction("aria_pool_reset_handle");
        if (!pool_reset_func) {
            llvm::FunctionType* pool_reset_type = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt64Ty()}, false);
            pool_reset_func = llvm::Function::Create(pool_reset_type,
                llvm::Function::ExternalLinkage, "aria_pool_reset_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        builder.CreateCall(pool_reset_func, {handle});
        return builder.getInt32(0);
    }
    
    if (callee_ident->name == "pool_destroy") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("pool_destroy() requires one argument");
        }
        
        llvm::Function* pool_destroy_func = module->getFunction("aria_pool_destroy_handle");
        if (!pool_destroy_func) {
            llvm::FunctionType* pool_destroy_type = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt64Ty()}, false);
            pool_destroy_func = llvm::Function::Create(pool_destroy_type,
                llvm::Function::ExternalLinkage, "aria_pool_destroy_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        builder.CreateCall(pool_destroy_func, {handle});
        return builder.getInt32(0);
    }
    
    if (callee_ident->name == "pool_get_total_blocks") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("pool_get_total_blocks() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_pool_get_total_blocks_handle");
        if (!func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_pool_get_total_blocks_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(func, {handle}, "total_blocks");
    }
    
    if (callee_ident->name == "pool_get_used_blocks") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("pool_get_used_blocks() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_pool_get_used_blocks_handle");
        if (!func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_pool_get_used_blocks_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(func, {handle}, "used_blocks");
    }
    
    // ====================================================================
    // END POOL BUILTINS
    // ====================================================================
    
    // ====================================================================
    // SLAB ALLOCATOR BUILTINS (Phase 4.2.5.4)
    // ====================================================================
    
    if (callee_ident->name == "slab_new") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("slab_new() requires exactly two arguments");
        }
        
        llvm::Function* slab_new_func = module->getFunction("aria_slab_cache_new_handle");
        if (!slab_new_func) {
            llvm::FunctionType* slab_new_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty(), builder.getInt64Ty()}, false);
            slab_new_func = llvm::Function::Create(slab_new_type,
                llvm::Function::ExternalLinkage, "aria_slab_cache_new_handle", module);
        }
        
        llvm::Value* object_size = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* slab_size = codegenExpressionNode(expr->arguments[1].get(), this);
        if (!object_size->getType()->isIntegerTy(64)) {
            object_size = builder.CreateSExtOrTrunc(object_size, builder.getInt64Ty());
        }
        if (!slab_size->getType()->isIntegerTy(64)) {
            slab_size = builder.CreateSExtOrTrunc(slab_size, builder.getInt64Ty());
        }
        return builder.CreateCall(slab_new_func, {object_size, slab_size}, "slab_handle");
    }
    
    if (callee_ident->name == "slab_alloc") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("slab_alloc() requires one argument");
        }
        
        llvm::Function* slab_alloc_func = module->getFunction("aria_slab_cache_alloc_handle");
        if (!slab_alloc_func) {
            llvm::FunctionType* slab_alloc_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            slab_alloc_func = llvm::Function::Create(slab_alloc_type,
                llvm::Function::ExternalLinkage, "aria_slab_cache_alloc_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(slab_alloc_func, {handle}, "alloc_ptr");
    }
    
    if (callee_ident->name == "slab_free") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("slab_free() requires two arguments");
        }
        
        llvm::Function* slab_free_func = module->getFunction("aria_slab_cache_free_handle");
        if (!slab_free_func) {
            llvm::FunctionType* slab_free_type = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt64Ty(), builder.getInt64Ty()}, false);
            slab_free_func = llvm::Function::Create(slab_free_type,
                llvm::Function::ExternalLinkage, "aria_slab_cache_free_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        if (!ptr->getType()->isIntegerTy(64)) {
            ptr = builder.CreateSExtOrTrunc(ptr, builder.getInt64Ty());
        }
        builder.CreateCall(slab_free_func, {handle, ptr});
        return builder.getInt32(0);
    }
    
    if (callee_ident->name == "slab_destroy") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("slab_destroy() requires one argument");
        }
        
        llvm::Function* slab_destroy_func = module->getFunction("aria_slab_cache_destroy_handle");
        if (!slab_destroy_func) {
            llvm::FunctionType* slab_destroy_type = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt64Ty()}, false);
            slab_destroy_func = llvm::Function::Create(slab_destroy_type,
                llvm::Function::ExternalLinkage, "aria_slab_cache_destroy_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        builder.CreateCall(slab_destroy_func, {handle});
        return builder.getInt32(0);
    }
    
    if (callee_ident->name == "slab_get_total_objects") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("slab_get_total_objects() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_slab_cache_get_total_objects_handle");
        if (!func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_slab_cache_get_total_objects_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(func, {handle}, "total_objects");
    }
    
    if (callee_ident->name == "slab_get_allocated_objects") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("slab_get_allocated_objects() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_slab_cache_get_allocated_objects_handle");
        if (!func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_slab_cache_get_allocated_objects_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(func, {handle}, "allocated_objects");
    }

    // ====================================================================
    // USER STACK BUILTINS (v0.4.3 — Per-scope Implicit Typed LIFO)
    // ====================================================================
    // The handle is stored in named_values["__aria_ustack_handle"] as
    // a hidden alloca, invisible to user code. Each function scope gets
    // at most one stack, initialized by astack().

    // astack() or astack(capacity) — initialize scope stack
    if (callee_ident->name == "astack") {
        if (expr->arguments.size() > 1) {
            throw std::runtime_error("astack() takes 0 or 1 arguments (optional capacity)");
        }

        // v0.4.3+: Use fast (SMT-optimized) or regular variant
        const char* func_name = ustack_fast_mode ? "aria_ustack_new_fast" : "aria_ustack_new";
        llvm::Function* func = module->getFunction(func_name);
        if (!func) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                func_name, module);
        }

        // Capacity: user-specified or default (256 slots = 1 page)
        llvm::Value* capacity;
        if (expr->arguments.size() == 1) {
            capacity = codegenExpressionNode(expr->arguments[0].get(), this);
            if (!capacity->getType()->isIntegerTy(64)) {
                capacity = builder.CreateSExtOrTrunc(capacity, builder.getInt64Ty());
            }
        } else {
            capacity = builder.getInt64(256); // ARIA_USTACK_DEFAULT_CAPACITY
        }

        llvm::Value* handle = builder.CreateCall(func, {capacity}, "ustack_handle");

        // Store handle in a hidden alloca accessible to apush/apop/apeek
        llvm::AllocaInst* handle_alloca = builder.CreateAlloca(
            builder.getInt64Ty(), nullptr, "__aria_ustack_handle");
        builder.CreateStore(handle, handle_alloca);
        named_values["__aria_ustack_handle"] = handle_alloca;

        // Return the handle value (discarded when used as statement)
        return handle;
    }

    // apush(value) — push value onto implicit scope stack
    if (callee_ident->name == "apush") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("apush() requires exactly one argument (value)");
        }

        // Load hidden handle
        auto it = named_values.find("__aria_ustack_handle");
        if (it == named_values.end()) {
            throw std::runtime_error("apush() called without astack() in scope");
        }
        llvm::Value* handle = builder.CreateLoad(builder.getInt64Ty(), it->second, "ustack_h");

        llvm::Value* rawVal = codegenExpressionNode(expr->arguments[0].get(), this);

        // Convert value to i64
        llvm::Value* valAsI64 = nullptr;
        llvm::Type* valTy = rawVal->getType();
        if (valTy->isIntegerTy(8))       { valAsI64 = builder.CreateZExt(rawVal, builder.getInt64Ty()); }
        else if (valTy->isIntegerTy(16))  { valAsI64 = builder.CreateZExt(rawVal, builder.getInt64Ty()); }
        else if (valTy->isIntegerTy(32))  { valAsI64 = builder.CreateZExt(rawVal, builder.getInt64Ty()); }
        else if (valTy->isIntegerTy(64))  { valAsI64 = rawVal; }
        else if (valTy->isFloatTy())      { valAsI64 = builder.CreateBitCast(builder.CreateFPExt(rawVal, builder.getDoubleTy()), builder.getInt64Ty()); }
        else if (valTy->isDoubleTy())     { valAsI64 = builder.CreateBitCast(rawVal, builder.getInt64Ty()); }
        else if (valTy->isIntegerTy(1))   { valAsI64 = builder.CreateZExt(rawVal, builder.getInt64Ty()); }
        else if (valTy->isPointerTy())    { valAsI64 = builder.CreatePtrToInt(rawVal, builder.getInt64Ty()); }
        else {
            if (valTy->isIntegerTy()) {
                valAsI64 = builder.CreateZExtOrTrunc(rawVal, builder.getInt64Ty());
            } else {
                valAsI64 = builder.CreatePtrToInt(rawVal, builder.getInt64Ty());
            }
        }

        if (ustack_fast_mode) {
            // v0.4.3+: SMT-optimized fast push — no type tag
            llvm::Function* func = module->getFunction("aria_ustack_push_fast");
            if (!func) {
                llvm::FunctionType* ft = llvm::FunctionType::get(
                    builder.getVoidTy(),
                    {builder.getInt64Ty(), builder.getInt64Ty()},
                    false);
                func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                    "aria_ustack_push_fast", module);
            }
            builder.CreateCall(func, {handle, valAsI64});
        } else {
            // Regular tagged push
            llvm::Function* func = module->getFunction("aria_ustack_push");
            if (!func) {
                llvm::FunctionType* ft = llvm::FunctionType::get(
                    builder.getVoidTy(),
                    {builder.getInt64Ty(), builder.getInt64Ty(), builder.getInt64Ty()},
                    false);
                func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                    "aria_ustack_push", module);
            }

            // Determine type tag
            int64_t typeTag = 3; // default: int64
            if (valTy->isIntegerTy(8))       { typeTag = 0; }
            else if (valTy->isIntegerTy(16))  { typeTag = 1; }
            else if (valTy->isIntegerTy(32))  { typeTag = 2; }
            else if (valTy->isIntegerTy(64))  { typeTag = 3; }
            else if (valTy->isFloatTy())      { typeTag = 4; }
            else if (valTy->isDoubleTy())     { typeTag = 5; }
            else if (valTy->isIntegerTy(1))   { typeTag = 6; }
            else if (valTy->isPointerTy())    { typeTag = 7; }
            else                              { typeTag = 8; }

            llvm::Value* tagVal = builder.getInt64(typeTag);
            builder.CreateCall(func, {handle, valAsI64, tagVal});
        }

        // Return a dummy int64 zero (push is effectively void; value is discarded)
        return builder.getInt64(0);
    }

    // apop() — pop typed value from implicit scope stack
    if (callee_ident->name == "apop") {
        if (!expr->arguments.empty()) {
            throw std::runtime_error("apop() takes no arguments (uses implicit scope stack)");
        }

        // Load hidden handle
        auto it = named_values.find("__aria_ustack_handle");
        if (it == named_values.end()) {
            throw std::runtime_error("apop() called without astack() in scope");
        }
        llvm::Value* handle = builder.CreateLoad(builder.getInt64Ty(), it->second, "ustack_h");

        llvm::Value* rawVal;
        if (ustack_fast_mode) {
            // v0.4.3+: SMT-optimized fast pop — no type check
            llvm::Function* func = module->getFunction("aria_ustack_pop_fast");
            if (!func) {
                llvm::FunctionType* ft = llvm::FunctionType::get(
                    builder.getInt64Ty(), {builder.getInt64Ty()}, false);
                func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                    "aria_ustack_pop_fast", module);
            }
            rawVal = builder.CreateCall(func, {handle}, "ustack_pop");
        } else {
            // Regular tagged pop
            llvm::Function* func = module->getFunction("aria_ustack_pop");
            if (!func) {
                llvm::FunctionType* ft = llvm::FunctionType::get(
                    builder.getInt64Ty(),
                    {builder.getInt64Ty(), builder.getInt64Ty()},
                    false);
                func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                    "aria_ustack_pop", module);
            }

            // Determine expected type tag from destination type context
            int64_t expectedTag = -1;
            llvm::Type* destType = ustack_pop_dest_type;

            if (destType) {
                if (destType->isIntegerTy(8))       expectedTag = 0;
                else if (destType->isIntegerTy(16)) expectedTag = 1;
                else if (destType->isIntegerTy(32)) expectedTag = 2;
                else if (destType->isIntegerTy(64)) expectedTag = 3;
                else if (destType->isFloatTy())     expectedTag = 4;
                else if (destType->isDoubleTy())    expectedTag = 5;
                else if (destType->isIntegerTy(1))  expectedTag = 6;
                else if (destType->isPointerTy())   expectedTag = 7;
            }

            llvm::Value* tag = builder.getInt64(expectedTag);
            rawVal = builder.CreateCall(func, {handle, tag}, "ustack_pop");
        }

        // Convert i64 to destination type
        llvm::Type* destType = ustack_pop_dest_type;
        if (destType && destType != builder.getInt64Ty()) {
            if (destType->isIntegerTy()) {
                rawVal = builder.CreateTrunc(rawVal, destType, "ustack_pop_trunc");
            } else if (destType->isFloatTy()) {
                llvm::Value* asDouble = builder.CreateBitCast(rawVal, builder.getDoubleTy(), "ustack_pop_d");
                rawVal = builder.CreateFPTrunc(asDouble, destType, "ustack_pop_f32");
            } else if (destType->isDoubleTy()) {
                rawVal = builder.CreateBitCast(rawVal, destType, "ustack_pop_f64");
            } else if (destType->isPointerTy()) {
                rawVal = builder.CreateIntToPtr(rawVal, destType, "ustack_pop_ptr");
            }
        }

        return rawVal;
    }

    // apeek() — peek typed value from implicit scope stack (non-destructive)
    if (callee_ident->name == "apeek") {
        if (!expr->arguments.empty()) {
            throw std::runtime_error("apeek() takes no arguments (uses implicit scope stack)");
        }

        // Load hidden handle
        auto it = named_values.find("__aria_ustack_handle");
        if (it == named_values.end()) {
            throw std::runtime_error("apeek() called without astack() in scope");
        }
        llvm::Value* handle = builder.CreateLoad(builder.getInt64Ty(), it->second, "ustack_h");

        llvm::Value* rawVal;
        if (ustack_fast_mode) {
            // v0.4.3+: SMT-optimized fast peek — no type check
            llvm::Function* func = module->getFunction("aria_ustack_peek_fast");
            if (!func) {
                llvm::FunctionType* ft = llvm::FunctionType::get(
                    builder.getInt64Ty(), {builder.getInt64Ty()}, false);
                func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                    "aria_ustack_peek_fast", module);
            }
            rawVal = builder.CreateCall(func, {handle}, "ustack_peek");
        } else {
            // Regular tagged peek
            llvm::Function* func = module->getFunction("aria_ustack_peek");
            if (!func) {
                llvm::FunctionType* ft = llvm::FunctionType::get(
                    builder.getInt64Ty(),
                    {builder.getInt64Ty(), builder.getInt64Ty()},
                    false);
                func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                    "aria_ustack_peek", module);
            }

            int64_t expectedTag = -1;
            llvm::Type* destType = ustack_pop_dest_type;

            if (destType) {
                if (destType->isIntegerTy(8))       expectedTag = 0;
                else if (destType->isIntegerTy(16)) expectedTag = 1;
                else if (destType->isIntegerTy(32)) expectedTag = 2;
                else if (destType->isIntegerTy(64)) expectedTag = 3;
                else if (destType->isFloatTy())     expectedTag = 4;
                else if (destType->isDoubleTy())    expectedTag = 5;
                else if (destType->isIntegerTy(1))  expectedTag = 6;
                else if (destType->isPointerTy())   expectedTag = 7;
            }

            llvm::Value* tag = builder.getInt64(expectedTag);
            rawVal = builder.CreateCall(func, {handle, tag}, "ustack_peek");
        }

        // Convert i64 to destination type
        llvm::Type* destType = ustack_pop_dest_type;
        if (destType && destType != builder.getInt64Ty()) {
            if (destType->isIntegerTy()) {
                rawVal = builder.CreateTrunc(rawVal, destType, "ustack_peek_trunc");
            } else if (destType->isFloatTy()) {
                llvm::Value* asDouble = builder.CreateBitCast(rawVal, builder.getDoubleTy(), "ustack_peek_d");
                rawVal = builder.CreateFPTrunc(asDouble, destType, "ustack_peek_f32");
            } else if (destType->isDoubleTy()) {
                rawVal = builder.CreateBitCast(rawVal, destType, "ustack_peek_f64");
            } else if (destType->isPointerTy()) {
                rawVal = builder.CreateIntToPtr(rawVal, destType, "ustack_peek_ptr");
            }
        }

        return rawVal;
    }

    // acap() — return stack capacity in bytes
    if (callee_ident->name == "acap") {
        auto it = named_values.find("__aria_ustack_handle");
        if (it == named_values.end()) {
            throw std::runtime_error("acap() called without astack() in scope");
        }
        llvm::Value* handle = builder.CreateLoad(builder.getInt64Ty(), it->second, "ustack_h");

        // Same function for both regular and fast mode — both structs store data_bytes
        const char* func_name = "aria_ustack_capacity_bytes";
        llvm::Function* func = module->getFunction(func_name);
        if (!func) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                func_name, module);
        }
        return builder.CreateCall(func, {handle}, "ustack_cap");
    }

    // asize() — return bytes currently used on stack
    if (callee_ident->name == "asize") {
        auto it = named_values.find("__aria_ustack_handle");
        if (it == named_values.end()) {
            throw std::runtime_error("asize() called without astack() in scope");
        }
        llvm::Value* handle = builder.CreateLoad(builder.getInt64Ty(), it->second, "ustack_h");

        // Regular: size * 16 (value + tag per slot), Fast: size * 8 (value only)
        const char* func_name = ustack_fast_mode ? "aria_ustack_bytes_used_fast" : "aria_ustack_bytes_used";
        llvm::Function* func = module->getFunction(func_name);
        if (!func) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                func_name, module);
        }
        return builder.CreateCall(func, {handle}, "ustack_used");
    }

    // afits(val) — check if value would fit on stack without overflow
    if (callee_ident->name == "afits") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("afits() requires exactly one argument (value to check)");
        }

        // Evaluate the argument for side effects, discard the value
        codegenExpressionNode(expr->arguments[0].get(), this);

        auto it = named_values.find("__aria_ustack_handle");
        if (it == named_values.end()) {
            throw std::runtime_error("afits() called without astack() in scope");
        }
        llvm::Value* handle = builder.CreateLoad(builder.getInt64Ty(), it->second, "ustack_h");

        // Same function for both modes — both structs have size/capacity at same offsets
        const char* func_name = "aria_ustack_fits";
        llvm::Function* func = module->getFunction(func_name);
        if (!func) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                func_name, module);
        }
        llvm::Value* result = builder.CreateCall(func, {handle}, "ustack_fits");
        return builder.CreateTrunc(result, builder.getInt1Ty(), "ustack_fits_bool");
    }

    // atype() — return type tag of top stack item (-1 if empty or fast mode)
    if (callee_ident->name == "atype") {
        auto it = named_values.find("__aria_ustack_handle");
        if (it == named_values.end()) {
            throw std::runtime_error("atype() called without astack() in scope");
        }
        llvm::Value* handle = builder.CreateLoad(builder.getInt64Ty(), it->second, "ustack_h");

        const char* func_name = ustack_fast_mode ? "aria_ustack_top_type_fast" : "aria_ustack_top_type";
        llvm::Function* func = module->getFunction(func_name);
        if (!func) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                func_name, module);
        }
        return builder.CreateCall(func, {handle}, "ustack_type");
    }

    // ====================================================================
    // USER HASH TABLE BUILTINS (v0.4.5 — Explicit-Handle String-Keyed Hash)
    // ====================================================================
    // Unlike astack (hidden handle), ahash uses explicit handles.
    // The handle is returned to the user and passed as first arg to all ops.
    // Auto-cleanup: tracked via __aria_uhash_handle_N named_values entries.

    // ahash(capacity) — create a new hash table, return handle
    if (callee_ident->name == "ahash") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("ahash() requires exactly one argument (capacity_bytes)");
        }

        const char* func_name = uhash_fast_mode ? "aria_uhash_new_fast" : "aria_uhash_new";
        llvm::Function* func = module->getFunction(func_name);
        if (!func) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                func_name, module);
        }

        llvm::Value* capacity = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!capacity->getType()->isIntegerTy(64)) {
            capacity = builder.CreateSExtOrTrunc(capacity, builder.getInt64Ty());
        }

        llvm::Value* handle = builder.CreateCall(func, {capacity}, "uhash_handle");

        // Track for auto-cleanup: store handle in a hidden alloca
        int idx = 0;
        if (uhash_handle_counter_ptr) {
            idx = (*uhash_handle_counter_ptr)++;
        }
        std::string trackKey = "__aria_uhash_handle_" + std::to_string(idx);
        llvm::AllocaInst* trackAlloca = builder.CreateAlloca(
            builder.getInt64Ty(), nullptr, trackKey);
        builder.CreateStore(handle, trackAlloca);
        named_values[trackKey] = trackAlloca;

        return handle;
    }

    // ahset(handle, key, value) — upsert key/value pair
    if (callee_ident->name == "ahset") {
        if (expr->arguments.size() != 3) {
            throw std::runtime_error("ahset() requires exactly 3 arguments (handle, key, value)");
        }

        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }

        // Key: extract raw char* from AriaString (struct pointer or struct value)
        llvm::Value* key = codegenExpressionNode(expr->arguments[1].get(), this);
        if (key->getType()->isPointerTy()) {
            // Pointer to AriaString struct → load struct, extract data ptr
            llvm::StructType* ast = llvm::StructType::getTypeByName(context, "struct.AriaString");
            if (!ast) {
                ast = llvm::StructType::create(context, {
                    builder.getPtrTy(), builder.getInt64Ty()
                }, "struct.AriaString");
            }
            llvm::Value* ss = builder.CreateLoad(ast, key, "uhash_key_struct");
            key = builder.CreateExtractValue(ss, 0, "uhash_key_data");
        } else if (key->getType()->isStructTy()) {
            // AriaString by value → extract data ptr directly
            key = builder.CreateExtractValue(key, 0, "uhash_key_data");
        } else {
            throw std::runtime_error("ahset() key argument must be a string");
        }

        // Value: box to i64  (same pattern as apush)
        llvm::Value* rawVal = codegenExpressionNode(expr->arguments[2].get(), this);
        llvm::Type* valTy = rawVal->getType();

        llvm::Value* valAsI64 = nullptr;
        if      (valTy->isIntegerTy(8))  { valAsI64 = builder.CreateZExt(rawVal, builder.getInt64Ty()); }
        else if (valTy->isIntegerTy(16)) { valAsI64 = builder.CreateZExt(rawVal, builder.getInt64Ty()); }
        else if (valTy->isIntegerTy(32)) { valAsI64 = builder.CreateZExt(rawVal, builder.getInt64Ty()); }
        else if (valTy->isIntegerTy(64)) { valAsI64 = rawVal; }
        else if (valTy->isFloatTy())     { valAsI64 = builder.CreateBitCast(builder.CreateFPExt(rawVal, builder.getDoubleTy()), builder.getInt64Ty()); }
        else if (valTy->isDoubleTy())    { valAsI64 = builder.CreateBitCast(rawVal, builder.getInt64Ty()); }
        else if (valTy->isIntegerTy(1))  { valAsI64 = builder.CreateZExt(rawVal, builder.getInt64Ty()); }
        else if (valTy->isPointerTy())   { valAsI64 = builder.CreatePtrToInt(rawVal, builder.getInt64Ty()); }
        else {
            if (valTy->isIntegerTy()) {
                valAsI64 = builder.CreateZExtOrTrunc(rawVal, builder.getInt64Ty());
            } else {
                valAsI64 = builder.CreatePtrToInt(rawVal, builder.getInt64Ty());
            }
        }

        // Type tag
        int64_t typeTag = 3; // default: int64
        if      (valTy->isIntegerTy(8))  { typeTag = 0; }
        else if (valTy->isIntegerTy(16)) { typeTag = 1; }
        else if (valTy->isIntegerTy(32)) { typeTag = 2; }
        else if (valTy->isIntegerTy(64)) { typeTag = 3; }
        else if (valTy->isFloatTy())     { typeTag = 4; }
        else if (valTy->isDoubleTy())    { typeTag = 5; }
        else if (valTy->isIntegerTy(1))  { typeTag = 6; }
        else if (valTy->isPointerTy())   { typeTag = 7; }
        else                             { typeTag = 8; }

        // Value size (bytes) for capacity accounting
        int64_t valueSize = 8; // default
        if (valTy->isIntegerTy(8))       { valueSize = 1; }
        else if (valTy->isIntegerTy(16)) { valueSize = 2; }
        else if (valTy->isIntegerTy(32)) { valueSize = 4; }
        else if (valTy->isIntegerTy(64)) { valueSize = 8; }
        else if (valTy->isFloatTy())     { valueSize = 4; }
        else if (valTy->isDoubleTy())    { valueSize = 8; }
        else if (valTy->isIntegerTy(1))  { valueSize = 1; }
        else if (valTy->isPointerTy())   { valueSize = 8; }

        if (uhash_fast_mode) {
            // Fast path — no type tag
            llvm::Function* func = module->getFunction("aria_uhash_set_fast");
            if (!func) {
                llvm::FunctionType* ft = llvm::FunctionType::get(
                    builder.getInt32Ty(),
                    {builder.getInt64Ty(), builder.getPtrTy(),
                     builder.getInt64Ty(), builder.getInt64Ty()},
                    false);
                func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                    "aria_uhash_set_fast", module);
            }
            llvm::Value* sizeVal = builder.getInt64(valueSize);
            return builder.CreateCall(func, {handle, key, valAsI64, sizeVal}, "uhash_set");
        } else {
            // Regular path with tag
            llvm::Function* func = module->getFunction("aria_uhash_set");
            if (!func) {
                llvm::FunctionType* ft = llvm::FunctionType::get(
                    builder.getInt32Ty(),
                    {builder.getInt64Ty(), builder.getPtrTy(),
                     builder.getInt64Ty(), builder.getInt64Ty(), builder.getInt64Ty()},
                    false);
                func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                    "aria_uhash_set", module);
            }
            llvm::Value* tagVal = builder.getInt64(typeTag);
            llvm::Value* sizeVal = builder.getInt64(valueSize);
            return builder.CreateCall(func, {handle, key, valAsI64, tagVal, sizeVal}, "uhash_set");
        }
    }

    // ahget(handle, key) — get value by key, type-checked against dest type
    if (callee_ident->name == "ahget") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("ahget() requires exactly 2 arguments (handle, key)");
        }

        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }

        llvm::Value* key = codegenExpressionNode(expr->arguments[1].get(), this);
        if (key->getType()->isPointerTy()) {
            llvm::StructType* ast = llvm::StructType::getTypeByName(context, "struct.AriaString");
            if (!ast) {
                ast = llvm::StructType::create(context, {
                    builder.getPtrTy(), builder.getInt64Ty()
                }, "struct.AriaString");
            }
            llvm::Value* ss = builder.CreateLoad(ast, key, "uhash_key_struct");
            key = builder.CreateExtractValue(ss, 0, "uhash_key_data");
        } else if (key->getType()->isStructTy()) {
            key = builder.CreateExtractValue(key, 0, "uhash_key_data");
        } else {
            throw std::runtime_error("ahget() key argument must be a string");
        }

        llvm::Value* rawVal;
        if (uhash_fast_mode) {
            llvm::Function* func = module->getFunction("aria_uhash_get_fast");
            if (!func) {
                llvm::FunctionType* ft = llvm::FunctionType::get(
                    builder.getInt64Ty(),
                    {builder.getInt64Ty(), builder.getPtrTy()},
                    false);
                func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                    "aria_uhash_get_fast", module);
            }
            rawVal = builder.CreateCall(func, {handle, key}, "uhash_get");
        } else {
            llvm::Function* func = module->getFunction("aria_uhash_get");
            if (!func) {
                llvm::FunctionType* ft = llvm::FunctionType::get(
                    builder.getInt64Ty(),
                    {builder.getInt64Ty(), builder.getPtrTy(), builder.getInt64Ty()},
                    false);
                func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                    "aria_uhash_get", module);
            }

            // Determine expected type tag from destination type context
            int64_t expectedTag = -1;
            llvm::Type* destType = uhash_get_dest_type;
            if (destType) {
                if      (destType->isIntegerTy(8))  expectedTag = 0;
                else if (destType->isIntegerTy(16)) expectedTag = 1;
                else if (destType->isIntegerTy(32)) expectedTag = 2;
                else if (destType->isIntegerTy(64)) expectedTag = 3;
                else if (destType->isFloatTy())     expectedTag = 4;
                else if (destType->isDoubleTy())    expectedTag = 5;
                else if (destType->isIntegerTy(1))  expectedTag = 6;
                else if (destType->isPointerTy())   expectedTag = 7;
            }

            llvm::Value* tag = builder.getInt64(expectedTag);
            rawVal = builder.CreateCall(func, {handle, key, tag}, "uhash_get");
        }

        // Convert i64 result to destination type
        llvm::Type* destType = uhash_get_dest_type;
        if (destType && destType != builder.getInt64Ty()) {
            if (destType->isIntegerTy()) {
                rawVal = builder.CreateTrunc(rawVal, destType, "uhash_get_trunc");
            } else if (destType->isFloatTy()) {
                llvm::Value* asDouble = builder.CreateBitCast(rawVal, builder.getDoubleTy(), "uhash_get_d");
                rawVal = builder.CreateFPTrunc(asDouble, destType, "uhash_get_f32");
            } else if (destType->isDoubleTy()) {
                rawVal = builder.CreateBitCast(rawVal, destType, "uhash_get_f64");
            } else if (destType->isPointerTy()) {
                rawVal = builder.CreateIntToPtr(rawVal, destType, "uhash_get_ptr");
            }
        }

        return rawVal;
    }

    // ahcount(handle) — return entry count
    if (callee_ident->name == "ahcount") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("ahcount() requires exactly 1 argument (handle)");
        }

        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }

        const char* func_name = uhash_fast_mode ? "aria_uhash_count_fast" : "aria_uhash_count";
        llvm::Function* func = module->getFunction(func_name);
        if (!func) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                func_name, module);
        }
        return builder.CreateCall(func, {handle}, "uhash_count");
    }

    // ahsize(handle) — return bytes used by stored values
    if (callee_ident->name == "ahsize") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("ahsize() requires exactly 1 argument (handle)");
        }

        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }

        const char* func_name = uhash_fast_mode ? "aria_uhash_size_fast" : "aria_uhash_size";
        llvm::Function* func = module->getFunction(func_name);
        if (!func) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                func_name, module);
        }
        return builder.CreateCall(func, {handle}, "uhash_size");
    }

    // ahfits(handle, value_size) — check if value_size bytes fit in remaining capacity
    if (callee_ident->name == "ahfits") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("ahfits() requires exactly 2 arguments (handle, value_size)");
        }

        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }

        llvm::Value* vsize = codegenExpressionNode(expr->arguments[1].get(), this);
        if (!vsize->getType()->isIntegerTy(64)) {
            vsize = builder.CreateSExtOrTrunc(vsize, builder.getInt64Ty());
        }

        const char* func_name = uhash_fast_mode ? "aria_uhash_fits_fast" : "aria_uhash_fits";
        llvm::Function* func = module->getFunction(func_name);
        if (!func) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getInt64Ty(),
                {builder.getInt64Ty(), builder.getInt64Ty()},
                false);
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                func_name, module);
        }
        llvm::Value* result = builder.CreateCall(func, {handle, vsize}, "uhash_fits");
        return builder.CreateTrunc(result, builder.getInt1Ty(), "uhash_fits_bool");
    }

    // ahtype(handle, key) — return type tag of value at key (-1 if not found)
    if (callee_ident->name == "ahtype") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("ahtype() requires exactly 2 arguments (handle, key)");
        }

        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }

        llvm::Value* key = codegenExpressionNode(expr->arguments[1].get(), this);
        if (key->getType()->isPointerTy()) {
            llvm::StructType* ast = llvm::StructType::getTypeByName(context, "struct.AriaString");
            if (!ast) {
                ast = llvm::StructType::create(context, {
                    builder.getPtrTy(), builder.getInt64Ty()
                }, "struct.AriaString");
            }
            llvm::Value* ss = builder.CreateLoad(ast, key, "uhash_key_struct");
            key = builder.CreateExtractValue(ss, 0, "uhash_key_data");
        } else if (key->getType()->isStructTy()) {
            key = builder.CreateExtractValue(key, 0, "uhash_key_data");
        } else {
            throw std::runtime_error("ahtype() key argument must be a string");
        }

        // ahtype only has regular variant (no fast — fast tables have no type tags)
        llvm::Function* func = module->getFunction("aria_uhash_type");
        if (!func) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getInt32Ty(),
                {builder.getInt64Ty(), builder.getPtrTy()},
                false);
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                "aria_uhash_type", module);
        }
        llvm::Value* result = builder.CreateCall(func, {handle, key}, "uhash_type");
        return builder.CreateSExt(result, builder.getInt64Ty(), "uhash_type_i64");
    }

    // ahdelete(handle, key) — delete key from table (0=ok, -1=not found)
    if (callee_ident->name == "ahdelete") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("ahdelete() requires exactly 2 arguments (handle, key)");
        }

        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }

        llvm::Value* key = codegenExpressionNode(expr->arguments[1].get(), this);
        if (key->getType()->isPointerTy()) {
            llvm::StructType* ast = llvm::StructType::getTypeByName(context, "struct.AriaString");
            if (!ast) {
                ast = llvm::StructType::create(context, {
                    builder.getPtrTy(), builder.getInt64Ty()
                }, "struct.AriaString");
            }
            llvm::Value* ss = builder.CreateLoad(ast, key, "uhash_key_struct");
            key = builder.CreateExtractValue(ss, 0, "uhash_key_data");
        } else if (key->getType()->isStructTy()) {
            key = builder.CreateExtractValue(key, 0, "uhash_key_data");
        } else {
            throw std::runtime_error("ahdelete() key argument must be a string");
        }

        const char* func_name = uhash_fast_mode ? "aria_uhash_delete_fast" : "aria_uhash_delete";
        llvm::Function* func = module->getFunction(func_name);
        if (!func) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getInt32Ty(),
                {builder.getInt64Ty(), builder.getPtrTy()},
                false);
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                func_name, module);
        }
        return builder.CreateCall(func, {handle, key}, "uhash_delete");
    }

    // ahhas(handle, key) — check if key exists (1=yes, 0=no)
    if (callee_ident->name == "ahhas") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("ahhas() requires exactly 2 arguments (handle, key)");
        }

        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }

        llvm::Value* key = codegenExpressionNode(expr->arguments[1].get(), this);
        if (key->getType()->isPointerTy()) {
            llvm::StructType* ast = llvm::StructType::getTypeByName(context, "struct.AriaString");
            if (!ast) {
                ast = llvm::StructType::create(context, {
                    builder.getPtrTy(), builder.getInt64Ty()
                }, "struct.AriaString");
            }
            llvm::Value* ss = builder.CreateLoad(ast, key, "uhash_key_struct");
            key = builder.CreateExtractValue(ss, 0, "uhash_key_data");
        } else if (key->getType()->isStructTy()) {
            key = builder.CreateExtractValue(key, 0, "uhash_key_data");
        } else {
            throw std::runtime_error("ahhas() key argument must be a string");
        }

        const char* func_name = uhash_fast_mode ? "aria_uhash_has_fast" : "aria_uhash_has";
        llvm::Function* func = module->getFunction(func_name);
        if (!func) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getInt32Ty(),
                {builder.getInt64Ty(), builder.getPtrTy()},
                false);
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                func_name, module);
        }
        llvm::Value* result = builder.CreateCall(func, {handle, key}, "uhash_has");
        return builder.CreateTrunc(result, builder.getInt1Ty(), "uhash_has_bool");
    }

    // ahclear(handle) — clear all entries from table
    if (callee_ident->name == "ahclear") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("ahclear() requires exactly 1 argument (handle)");
        }

        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }

        const char* func_name = uhash_fast_mode ? "aria_uhash_clear_fast" : "aria_uhash_clear";
        llvm::Function* func = module->getFunction(func_name);
        if (!func) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getVoidTy(),
                {builder.getInt64Ty()},
                false);
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                func_name, module);
        }
        return builder.CreateCall(func, {handle});
    }

    // ahkeys(handle) — get array of all keys (returns pointer to char** array)
    if (callee_ident->name == "ahkeys") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("ahkeys() requires exactly 1 argument (handle)");
        }

        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }

        // Allocate stack space for the out_count parameter
        llvm::Value* countPtr = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "ahkeys_count");

        const char* func_name = uhash_fast_mode ? "aria_uhash_keys_fast" : "aria_uhash_keys";
        llvm::Function* func = module->getFunction(func_name);
        if (!func) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getPtrTy(),
                {builder.getInt64Ty(), builder.getPtrTy()},
                false);
            func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                func_name, module);
        }
        llvm::Value* keysPtr = builder.CreateCall(func, {handle, countPtr}, "uhash_keys");
        // Return the pointer as int64 (handle-style)
        return builder.CreatePtrToInt(keysPtr, builder.getInt64Ty(), "uhash_keys_i64");
    }

    // ====================================================================
    // WILD MEMORY BUILTINS (Phase 2.2 - Manual Memory Management)
    // ====================================================================
    // Primitive wild memory operations tracked by the borrow checker.
    // Runtime functions: aria_alloc, aria_free, aria_realloc (wild_alloc.cpp)

    // alloc(size: int64) -> wild int8@
    // Allocates 'size' bytes of wild (manual) memory
    if (callee_ident->name == "alloc") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("alloc() requires exactly one argument (size)");
        }

        // Get or declare aria_alloc: void* aria_alloc(size_t size)
        llvm::Function* alloc_func = module->getFunction("aria_alloc");
        if (!alloc_func) {
            llvm::FunctionType* alloc_type = llvm::FunctionType::get(
                builder.getPtrTy(),                    // Return: void* (opaque ptr)
                {builder.getInt64Ty()},                // Args: size_t size
                false);
            alloc_func = llvm::Function::Create(alloc_type,
                llvm::Function::ExternalLinkage, "aria_alloc", module);
        }

        llvm::Value* size = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Handle LBIM types (int128/256/512) which are represented as structs
        if (size->getType()->isStructTy()) {
            // Extract the first limb (low 64 bits) from the LBIM struct
            // struct.intN = { [M x i64] } where M = N/64
            size = builder.CreateExtractValue(size, {0, 0}, "size.limb0");
        } else if (!size->getType()->isIntegerTy()) {
            throw std::runtime_error("alloc() size must be an integer type");
        }
        
        // Now convert to i64 if needed
        if (!size->getType()->isIntegerTy(64)) {
            size = builder.CreateZExtOrTrunc(size, builder.getInt64Ty(), "size.i64");
        }
        
        return builder.CreateCall(alloc_func, {size}, "wild_ptr");
    }

    // free(ptr: wild any@) -> void
    // Frees a wild pointer allocated by alloc()
    if (callee_ident->name == "free") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("free() requires exactly one argument (pointer)");
        }

        // Get or declare aria_free: void aria_free(void* ptr)
        llvm::Function* free_func = module->getFunction("aria_free");
        if (!free_func) {
            llvm::FunctionType* free_type = llvm::FunctionType::get(
                builder.getVoidTy(),                   // Return: void
                {builder.getPtrTy()},                  // Args: void* ptr
                false);
            free_func = llvm::Function::Create(free_type,
                llvm::Function::ExternalLinkage, "aria_free", module);
        }

        llvm::Value* ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        // Ensure ptr is a pointer type
        if (!ptr->getType()->isPointerTy()) {
            ptr = builder.CreateIntToPtr(ptr, builder.getPtrTy());
        }
        builder.CreateCall(free_func, {ptr});
        return builder.getInt32(0); // Return dummy value for void
    }

    // realloc(ptr: wild any@, new_size: int64) -> wild int8@
    // Reallocates a wild pointer to a new size
    if (callee_ident->name == "realloc") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("realloc() requires exactly two arguments (ptr, new_size)");
        }

        // Get or declare aria_realloc: void* aria_realloc(void* ptr, size_t new_size)
        llvm::Function* realloc_func = module->getFunction("aria_realloc");
        if (!realloc_func) {
            llvm::FunctionType* realloc_type = llvm::FunctionType::get(
                builder.getPtrTy(),                              // Return: void*
                {builder.getPtrTy(), builder.getInt64Ty()},      // Args: void* ptr, size_t size
                false);
            realloc_func = llvm::Function::Create(realloc_type,
                llvm::Function::ExternalLinkage, "aria_realloc", module);
        }

        llvm::Value* ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* new_size = codegenExpressionNode(expr->arguments[1].get(), this);

        if (!ptr->getType()->isPointerTy()) {
            ptr = builder.CreateIntToPtr(ptr, builder.getPtrTy());
        }
        if (!new_size->getType()->isIntegerTy(64)) {
            new_size = builder.CreateZExtOrTrunc(new_size, builder.getInt64Ty());
        }
        return builder.CreateCall(realloc_func, {ptr, new_size}, "realloc_ptr");
    }

    // ====================================================================
    // STRING LIBRARY BUILTINS (Phase 4.3)
    // ====================================================================
    
    // Note: AriaString struct is { const char* data, int64_t length }
    // In LLVM IR, this is { i8*, i64 }
    
    // Helper: Get or create AriaString struct type
    auto getAriaStringType = [&]() -> llvm::StructType* {
        llvm::StructType* strType = llvm::StructType::getTypeByName(context, "struct.AriaString");
        if (!strType) {
            std::vector<llvm::Type*> fields = {
                llvm::PointerType::get(builder.getInt8Ty(), 0),
                builder.getInt64Ty()
            };
            strType = llvm::StructType::create(context, fields, "struct.AriaString");
        }
        return strType;
    };
    
    // string_from_cstr(char*) -> string
    if (callee_ident->name == "string_from_cstr") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_from_cstr() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_string_from_cstr_simple");
        if (!func) {
            // aria_string_from_cstr_simple returns AriaString* directly (aborts on error)
            std::vector<llvm::Type*> params = {llvm::PointerType::get(builder.getInt8Ty(), 0)};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_from_cstr_simple", module);
        }
        
        llvm::Value* cstr = codegenExpressionNode(expr->arguments[0].get(), this);
        // Cast to i8* if needed
        if (!cstr->getType()->isPointerTy()) {
            cstr = builder.CreateIntToPtr(cstr, llvm::PointerType::get(builder.getInt8Ty(), 0));
        }
        return builder.CreateCall(func, {cstr}, "str_result");
    }
    
    // string_from_char(byte) -> string  
    if (callee_ident->name == "string_from_char") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_from_char() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_string_from_char_simple");
        if (!func) {
            // aria_string_from_char_simple returns AriaString* directly (aborts on error)
            std::vector<llvm::Type*> params = {builder.getInt8Ty()};  // uint8_t ch
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_from_char_simple", module);
        }
        
        llvm::Value* ch = codegenExpressionNode(expr->arguments[0].get(), this);
        // Truncate to i8 if needed
        if (!ch->getType()->isIntegerTy(8)) {
            ch = builder.CreateTrunc(ch, builder.getInt8Ty());
        }
        
        return builder.CreateCall(func, {ch}, "char_str");
    }
    
    // string_length(string) -> int64
    if (callee_ident->name == "string_length") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_length() requires one argument");
        }

        // codegenExpressionNode returns the loaded value (AriaString* for string variables)
        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        // Load the AriaString struct from that pointer
        llvm::Value* str_struct = builder.CreateLoad(getAriaStringType(), str_ptr, "str");
        // Extract length field (index 1)
        llvm::Value* length = builder.CreateExtractValue(str_struct, {1}, "length");
        return length;
    }
    
    // string_is_empty(string) -> bool
    if (callee_ident->name == "string_is_empty") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_is_empty() requires one argument");
        }
        
        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* str_struct = builder.CreateLoad(getAriaStringType(), str_ptr, "str");
        llvm::Value* length = builder.CreateExtractValue(str_struct, {1}, "length");
        llvm::Value* zero = builder.getInt64(0);
        return builder.CreateICmpEQ(length, zero, "is_empty");
    }
    
    // string_equals(string, string) -> bool
    if (callee_ident->name == "string_equals") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_equals() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_string_equals");
        if (!func) {
            std::vector<llvm::Type*> params = {getAriaStringType(), getAriaStringType()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt1Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_equals", module);
        }
        
        llvm::Value* str1_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* str2_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* str1 = builder.CreateLoad(getAriaStringType(), str1_ptr, "str1");
        llvm::Value* str2 = builder.CreateLoad(getAriaStringType(), str2_ptr, "str2");
        return builder.CreateCall(func, {str1, str2}, "equals");
    }
    
    // string_concat(string, string) -> string
    if (callee_ident->name == "string_concat") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_concat() requires two arguments");
        }

        llvm::Function* func = module->getFunction("aria_string_concat_simple");
        if (!func) {
            // aria_string_concat_simple returns AriaString* directly (aborts on error)
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(getAriaStringType(), 0),  // AriaString* a
                llvm::PointerType::get(getAriaStringType(), 0)   // AriaString* b
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_concat_simple", module);
        }

        // codegenExpressionNode returns loaded pointers (AriaString* for string variables)
        llvm::Value* str1_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* str2_ptr = codegenExpressionNode(expr->arguments[1].get(), this);

        // Auto-unwrap Result<string> = { ptr, ptr, i8 } from function calls like int32_toString()
        auto unwrapResultPtr = [&](llvm::Value* val) -> llvm::Value* {
            if (val->getType()->isStructTy()) {
                llvm::StructType* st = llvm::cast<llvm::StructType>(val->getType());
                if (st->getNumElements() == 3 &&
                    st->getElementType(1)->isPointerTy() &&
                    st->getElementType(2)->isIntegerTy(8)) {
                    return builder.CreateExtractValue(val, {0}, "unwrap_result_str");
                }
            }
            return val;
        };
        str1_ptr = unwrapResultPtr(str1_ptr);
        str2_ptr = unwrapResultPtr(str2_ptr);

        return builder.CreateCall(func, {str1_ptr, str2_ptr}, "concat_str");
    }
    
    // string_substring(string, int64, int64) -> string
    if (callee_ident->name == "string_substring") {
        if (expr->arguments.size() != 3) {
            throw std::runtime_error("string_substring() requires three arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_string_substring_simple");
        if (!func) {
            // aria_string_substring_simple(AriaString*, i64, i64) -> AriaString*
            // aborts on out-of-bounds (matches the _simple wrapper convention)
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(getAriaStringType(), 0),  // str
                builder.getInt64Ty(),                            // start
                builder.getInt64Ty()                             // end
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_substring_simple", module);
        }
        
        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* start = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* end = codegenExpressionNode(expr->arguments[2].get(), this);
        // Widen index args to i64 if needed
        if (start->getType() != builder.getInt64Ty())
            start = builder.CreateSExt(start, builder.getInt64Ty(), "substr_start_i64");
        if (end->getType() != builder.getInt64Ty())
            end = builder.CreateSExt(end, builder.getInt64Ty(), "substr_end_i64");
        return builder.CreateCall(func, {str_ptr, start, end}, "substr_result");
    }
    
    // string_contains(string, string) -> bool
    if (callee_ident->name == "string_contains") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_contains() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_string_contains");
        if (!func) {
            std::vector<llvm::Type*> params = {getAriaStringType(), getAriaStringType()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt1Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_contains", module);
        }
        
        llvm::Value* haystack_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* needle_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* haystack = builder.CreateLoad(getAriaStringType(), haystack_ptr, "haystack");
        llvm::Value* needle = builder.CreateLoad(getAriaStringType(), needle_ptr, "needle");
        return builder.CreateCall(func, {haystack, needle}, "contains");
    }
    
    // string_starts_with(string, string) -> bool
    if (callee_ident->name == "string_starts_with") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_starts_with() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_string_starts_with");
        if (!func) {
            std::vector<llvm::Type*> params = {getAriaStringType(), getAriaStringType()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt1Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_starts_with", module);
        }
        
        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* prefix_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* str = builder.CreateLoad(getAriaStringType(), str_ptr, "str");
        llvm::Value* prefix = builder.CreateLoad(getAriaStringType(), prefix_ptr, "prefix");
        return builder.CreateCall(func, {str, prefix}, "starts_with");
    }
    
    // string_ends_with(string, string) -> bool
    if (callee_ident->name == "string_ends_with") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_ends_with() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_string_ends_with");
        if (!func) {
            std::vector<llvm::Type*> params = {getAriaStringType(), getAriaStringType()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt1Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_ends_with", module);
        }
        
        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* suffix_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* str = builder.CreateLoad(getAriaStringType(), str_ptr, "str");
        llvm::Value* suffix = builder.CreateLoad(getAriaStringType(), suffix_ptr, "suffix");
        return builder.CreateCall(func, {str, suffix}, "ends_with");
    }
    
    // string_trim(string) -> string
    if (callee_ident->name == "string_trim") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_trim() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_string_trim_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(getAriaStringType(), 0)  // str (ptr)
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_trim_simple", module);
        }
        
        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {str_ptr}, "trim_result");
    }
    
    // string_to_upper(string) -> string
    if (callee_ident->name == "string_to_upper") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_to_upper() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_string_to_upper_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(getAriaStringType(), 0)  // str (ptr)
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_to_upper_simple", module);
        }
        
        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {str_ptr}, "upper_result");
    }
    
    // string_to_lower(string) -> string
    if (callee_ident->name == "string_to_lower") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_to_lower() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_string_to_lower_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(getAriaStringType(), 0)  // str (ptr)
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_to_lower_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {str_ptr}, "lower_result");
    }

    // string_from_int(int64) -> string
    if (callee_ident->name == "string_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_from_int() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_string_from_int_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getInt64Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_from_int_simple", module);
        }

        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        // Ensure value is i64 (sign extend if smaller)
        if (val->getType() != builder.getInt64Ty()) {
            val = builder.CreateSExt(val, builder.getInt64Ty(), "val_i64");
        }
        return builder.CreateCall(func, {val}, "from_int_result");
    }

    // string_to_int(string) -> int64
    if (callee_ident->name == "string_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_to_int() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_string_to_int_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_to_int_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {str_ptr}, "to_int_result");
    }

    // string_to_hex(string) -> string
    if (callee_ident->name == "string_to_hex") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_to_hex() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_string_to_hex_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_to_hex_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {str_ptr}, "to_hex_result");
    }

    // string_pad_right(string, int64, int8) -> string
    if (callee_ident->name == "string_pad_right") {
        if (expr->arguments.size() != 3) {
            throw std::runtime_error("string_pad_right() requires three arguments");
        }

        llvm::Function* func = module->getFunction("aria_string_pad_right_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getInt64Ty(), builder.getInt8Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_pad_right_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* total_len = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* pad_char = codegenExpressionNode(expr->arguments[2].get(), this);
        // Extend total_len to i64 if needed
        if (total_len->getType() != builder.getInt64Ty()) {
            total_len = builder.CreateSExt(total_len, builder.getInt64Ty(), "len_i64");
        }
        // Truncate pad_char to i8 if needed
        if (pad_char->getType() != builder.getInt8Ty()) {
            pad_char = builder.CreateTrunc(pad_char, builder.getInt8Ty(), "pad_i8");
        }
        return builder.CreateCall(func, {str_ptr, total_len, pad_char}, "pad_right_result");
    }

    // string_pad_left(string, int64, int8) -> string
    if (callee_ident->name == "string_pad_left") {
        if (expr->arguments.size() != 3) {
            throw std::runtime_error("string_pad_left() requires three arguments");
        }

        llvm::Function* func = module->getFunction("aria_string_pad_left_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getInt64Ty(), builder.getInt8Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_pad_left_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* total_len = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* pad_char = codegenExpressionNode(expr->arguments[2].get(), this);
        if (total_len->getType() != builder.getInt64Ty()) {
            total_len = builder.CreateSExt(total_len, builder.getInt64Ty(), "len_i64");
        }
        if (pad_char->getType() != builder.getInt8Ty()) {
            pad_char = builder.CreateTrunc(pad_char, builder.getInt8Ty(), "pad_i8");
        }
        return builder.CreateCall(func, {str_ptr, total_len, pad_char}, "pad_left_result");
    }

    // string_repeat(string, int64) -> string
    if (callee_ident->name == "string_repeat") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_repeat() requires two arguments");
        }

        llvm::Function* func = module->getFunction("aria_string_repeat_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getInt64Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_repeat_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* count = codegenExpressionNode(expr->arguments[1].get(), this);
        if (count->getType() != builder.getInt64Ty()) {
            count = builder.CreateSExt(count, builder.getInt64Ty(), "count_i64");
        }
        return builder.CreateCall(func, {str_ptr, count}, "repeat_result");
    }

    // string_trim_start(string) -> string
    if (callee_ident->name == "string_trim_start") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_trim_start() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_string_trim_start_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_trim_start_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {str_ptr}, "trim_start_result");
    }

    // string_trim_end(string) -> string
    if (callee_ident->name == "string_trim_end") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_trim_end() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_string_trim_end_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_trim_end_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {str_ptr}, "trim_end_result");
    }

    // string_index_of(string, string) -> int64  (-1 if not found)
    if (callee_ident->name == "string_index_of") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_index_of() requires two arguments");
        }

        llvm::Function* func = module->getFunction("aria_string_index_of_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_index_of_simple", module);
        }

        llvm::Value* haystack_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* needle_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        return builder.CreateCall(func, {haystack_ptr, needle_ptr}, "index_of_result");
    }

    // string_from_int_hex(int64) -> string (lowercase hex digits, no prefix)
    if (callee_ident->name == "string_from_int_hex") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_from_int_hex() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_string_from_int_hex_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getInt64Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_from_int_hex_simple", module);
        }

        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        if (val->getType() != builder.getInt64Ty()) {
            val = builder.CreateSExt(val, builder.getInt64Ty(), "hex_val_ext");
        }
        return builder.CreateCall(func, {val}, "from_int_hex_result");
    }

    // string_format_float(float64, int32) -> string
    if (callee_ident->name == "string_format_float") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_format_float() requires two arguments");
        }

        llvm::Function* func = module->getFunction("aria_string_format_float_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getDoubleTy(), builder.getInt32Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_format_float_simple", module);
        }

        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* precision = codegenExpressionNode(expr->arguments[1].get(), this);
        // Truncate precision to i32 if needed
        if (precision->getType() != builder.getInt32Ty()) {
            precision = builder.CreateTrunc(precision, builder.getInt32Ty(), "prec_i32");
        }
        return builder.CreateCall(func, {val, precision}, "format_float_result");
    }

    // ====================================================================
    // FILE I/O BUILTINS
    // ====================================================================

    // Helper to convert AriaString global to char*
    auto stringToCharPtr = [&](llvm::Value* str_val) -> llvm::Value* {
        // For global AriaString constants (string literals), extract the data field
        // Global AriaStrings are of type %struct.AriaString = { ptr, i64 }
        // We need the first field (the char* pointer)
        if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(str_val)) {
            // Use GEP to get pointer to first field (data pointer)
            std::vector<llvm::Value*> indices = {
                builder.getInt32(0),  // Deref the global pointer
                builder.getInt32(0)   // Access first struct field (data)
            };
            llvm::Value* data_ptr_gep = builder.CreateInBoundsGEP(
                getAriaStringType(), gv, indices, "str_data_ptr");
            // Load the char* pointer from the struct
            return builder.CreateLoad(builder.getPtrTy(), data_ptr_gep, "str_data");
        }
        
        // For stack/heap AriaString* variables, load struct and extract data
        if (str_val->getType()->isPointerTy()) {
            llvm::Value* str_struct = builder.CreateLoad(getAriaStringType(), str_val, "str");
            return builder.CreateExtractValue(str_struct, {0}, "str_data");
        }
        
        return str_val;
    };

    // aria_write_file_simple(path: int8*, content: int8*) -> int64
    if (callee_ident->name == "aria_write_file_simple") {
        ARIA_DBG_STREAM << "[FS DEBUG] aria_write_file_simple codegen called" << std::endl;
        
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("aria_write_file_simple() requires two arguments");
        }

        llvm::Function* func = module->getFunction("aria_write_file_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_write_file_simple", module);
        }

        llvm::Value* path = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* content = codegenExpressionNode(expr->arguments[1].get(), this);
        
        ARIA_DBG_STREAM << "[FS DEBUG] path type: " << path->getValueName()->getKey().str() << std::endl;
        
        // Convert AriaString to char* - for global constants, extract the data field
        if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(path)) {
            ARIA_DBG_STREAM << "[FS DEBUG] path is GlobalVariable" << std::endl;
            if (auto* init = gv->getInitializer()) {
                ARIA_DBG_STREAM << "[FS DEBUG] has initializer" << std::endl;
                if (auto* cs = llvm::dyn_cast<llvm::ConstantStruct>(init)) {
                    ARIA_DBG_STREAM << "[FS DEBUG] is ConstantStruct, extracting field 0" << std::endl;
                    // Extract the first field (data pointer) from the struct constant
                    path = cs->getOperand(0);
                    ARIA_DBG_STREAM << "[FS DEBUG] extracted path" << std::endl;
                }
            }
        }
        if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(content)) {
            if (auto* init = gv->getInitializer()) {
                if (auto* cs = llvm::dyn_cast<llvm::ConstantStruct>(init)) {
                    // Extract the first field (data pointer) from the struct constant
                    content = cs->getOperand(0);
                }
            }
        }
        
        ARIA_DBG_STREAM << "[FS DEBUG] calling with extracted values" << std::endl;
        return builder.CreateCall(func, {path, content}, "write_result");
    }

    // aria_file_exists(path: int8*) -> bool
    if (callee_ident->name == "aria_file_exists") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("aria_file_exists() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_file_exists");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt1Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_file_exists", module);
        }

        llvm::Value* path = codegenExpressionNode(expr->arguments[0].get(), this);
        path = stringToCharPtr(path);
        
        return builder.CreateCall(func, {path}, "exists_result");
    }

    // aria_delete_file_simple(path: int8*) -> int64
    if (callee_ident->name == "aria_delete_file_simple") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("aria_delete_file_simple() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_delete_file_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_delete_file_simple", module);
        }

        llvm::Value* path = codegenExpressionNode(expr->arguments[0].get(), this);
        path = stringToCharPtr(path);
        
        return builder.CreateCall(func, {path}, "delete_result");
    }

    // aria_read_file_simple(path: int8*) -> string
    if (callee_ident->name == "aria_read_file_simple") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("aria_read_file_simple() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_read_file_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_read_file_simple", module);
        }

        llvm::Value* path = codegenExpressionNode(expr->arguments[0].get(), this);
        path = stringToCharPtr(path);
        
        return builder.CreateCall(func, {path}, "read_result");
    }

    // ====================================================================
    // TBB (Tagged Balanced Base) TYPE BUILTINS
    // ====================================================================

    // Helper lambda for TBB ERR sentinel values
    auto getTBBErrSentinel = [&](unsigned bits) -> llvm::Value* {
        llvm::IntegerType* ty = llvm::IntegerType::get(context, bits);
        int64_t errVal;
        switch (bits) {
            case 8:  errVal = -128; break;
            case 16: errVal = -32768; break;
            case 32: errVal = -2147483648LL; break;
            case 64: errVal = INT64_MIN; break;
            default: errVal = INT64_MIN;
        }
        return llvm::ConstantInt::get(ty, errVal, true);
    };

    // tbb64_from_int(int64) -> tbb64
    if (callee_ident->name == "tbb64_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        // TBB64 uses INT64_MIN as ERR, so all int64 values except INT64_MIN are valid
        // If input is INT64_MIN, return ERR
        llvm::Value* errSentinel = getTBBErrSentinel(64);
        llvm::Value* isErr = builder.CreateICmpEQ(val, errSentinel, "is_min");
        return builder.CreateSelect(isErr, errSentinel, val, "tbb64_result");
    }

    // tbb64_is_err(tbb64) -> bool
    if (callee_ident->name == "tbb64_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_is_err() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* errSentinel = getTBBErrSentinel(64);
        return builder.CreateICmpEQ(val, errSentinel, "is_err");
    }

    // tbb64_to_int(tbb64) -> int64
    if (callee_ident->name == "tbb64_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_to_int() requires one argument");
        }
        // Simply return the value - caller should check is_err first
        return codegenExpressionNode(expr->arguments[0].get(), this);
    }

    // tbb32_from_int(int32) -> tbb32
    // ARIA SAFETY FIX (Gemini Batch 02, BUG-004): Range-check BEFORE truncation
    // Prevents wraparound (e.g., 2147483648 → -2147483648 instead of ERR)
    if (callee_ident->name == "tbb32_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // TBB32 valid range: [-2147483647, +2147483647] (symmetric)
        // Sentinel: -2147483648 (INT32_MIN)
        llvm::Value* errSentinel = getTBBErrSentinel(32);
        unsigned srcWidth = val->getType()->getIntegerBitWidth();
        
        if (srcWidth > 32) {
            // Source is wider than destination - MUST range check before truncation
            llvm::Value* tbb32_max = llvm::ConstantInt::get(val->getType(), 2147483647LL);
            llvm::Value* tbb32_min = llvm::ConstantInt::get(val->getType(), -2147483647LL);
            
            llvm::Value* tooHigh = builder.CreateICmpSGT(val, tbb32_max, "tbb32_too_high");
            llvm::Value* tooLow = builder.CreateICmpSLT(val, tbb32_min, "tbb32_too_low");
            llvm::Value* outOfRange = builder.CreateOr(tooHigh, tooLow, "tbb32_out_of_range");
            
            llvm::Value* truncated = builder.CreateTrunc(val, builder.getInt32Ty(), "trunc32");
            return builder.CreateSelect(outOfRange, errSentinel, truncated, "tbb32_safe");
        } else if (srcWidth < 32) {
            // Source is narrower - sign extend (safe, preserves value)
            val = builder.CreateSExt(val, builder.getInt32Ty(), "sext32");
        }
        // else: srcWidth == 32, already correct size
        
        // Final check: is the value itself the sentinel?
        llvm::Value* isErr = builder.CreateICmpEQ(val, errSentinel, "is_min");
        return builder.CreateSelect(isErr, errSentinel, val, "tbb32_result");
    }

    // tbb32_is_err(tbb32) -> bool
    if (callee_ident->name == "tbb32_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_is_err() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* errSentinel = getTBBErrSentinel(32);
        return builder.CreateICmpEQ(val, errSentinel, "is_err");
    }

    // tbb32_to_int(tbb32) -> int32
    if (callee_ident->name == "tbb32_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_to_int() requires one argument");
        }
        return codegenExpressionNode(expr->arguments[0].get(), this);
    }

    // tbb16_from_int(int16) -> tbb16
    // ARIA SAFETY FIX (Gemini Batch 02, BUG-004): Range-check BEFORE truncation
    // Prevents wraparound (e.g., 35000 → -30536 instead of ERR)
    if (callee_ident->name == "tbb16_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // TBB16 valid range: [-32767, +32767] (symmetric)
        // Sentinel: -32768 (INT16_MIN)
        llvm::Value* errSentinel = getTBBErrSentinel(16);
        unsigned srcWidth = val->getType()->getIntegerBitWidth();
        
        if (srcWidth > 16) {
            // Source is wider than destination - MUST range check before truncation
            llvm::Value* tbb16_max = llvm::ConstantInt::get(val->getType(), 32767);
            llvm::Value* tbb16_min = llvm::ConstantInt::get(val->getType(), -32767);
            
            llvm::Value* tooHigh = builder.CreateICmpSGT(val, tbb16_max, "tbb16_too_high");
            llvm::Value* tooLow = builder.CreateICmpSLT(val, tbb16_min, "tbb16_too_low");
            llvm::Value* outOfRange = builder.CreateOr(tooHigh, tooLow, "tbb16_out_of_range");
            
            llvm::Value* truncated = builder.CreateTrunc(val, builder.getInt16Ty(), "trunc16");
            return builder.CreateSelect(outOfRange, errSentinel, truncated, "tbb16_safe");
        } else if (srcWidth < 16) {
            // Source is narrower - sign extend (safe, preserves value)
            val = builder.CreateSExt(val, builder.getInt16Ty(), "sext16");
        }
        // else: srcWidth == 16, already correct size
        
        // Final check: is the value itself the sentinel?
        llvm::Value* isErr = builder.CreateICmpEQ(val, errSentinel, "is_min");
        return builder.CreateSelect(isErr, errSentinel, val, "tbb16_result");
    }

    // tbb16_is_err(tbb16) -> bool
    if (callee_ident->name == "tbb16_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_is_err() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* errSentinel = getTBBErrSentinel(16);
        return builder.CreateICmpEQ(val, errSentinel, "is_err");
    }

    // tbb16_to_int(tbb16) -> int16
    if (callee_ident->name == "tbb16_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_to_int() requires one argument");
        }
        return codegenExpressionNode(expr->arguments[0].get(), this);
    }

    // tbb8_from_int(int8) -> tbb8
    // ARIA SAFETY FIX (Gemini Batch 02, BUG-004): Range-check BEFORE truncation
    // Prevents wraparound (e.g., 300 → 44 instead of ERR)
    // Physical consequence prevented: Force limit bypass in robot control
    if (callee_ident->name == "tbb8_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // TBB8 valid range: [-127, +127] (symmetric)
        // Sentinel: -128 (INT8_MIN)
        llvm::Value* errSentinel = getTBBErrSentinel(8);
        unsigned srcWidth = val->getType()->getIntegerBitWidth();
        
        if (srcWidth > 8) {
            // Source is wider than destination - MUST range check before truncation
            llvm::Value* tbb8_max = llvm::ConstantInt::get(val->getType(), 127);
            llvm::Value* tbb8_min = llvm::ConstantInt::get(val->getType(), -127);
            
            llvm::Value* tooHigh = builder.CreateICmpSGT(val, tbb8_max, "tbb8_too_high");
            llvm::Value* tooLow = builder.CreateICmpSLT(val, tbb8_min, "tbb8_too_low");
            llvm::Value* outOfRange = builder.CreateOr(tooHigh, tooLow, "tbb8_out_of_range");
            
            llvm::Value* truncated = builder.CreateTrunc(val, builder.getInt8Ty(), "trunc8");
            return builder.CreateSelect(outOfRange, errSentinel, truncated, "tbb8_safe");
        }
        // else: srcWidth <= 8, already fits (i8 or smaller)
        
        // Final check: is the value itself the sentinel?
        llvm::Value* isErr = builder.CreateICmpEQ(val, errSentinel, "is_min");
        return builder.CreateSelect(isErr, errSentinel, val, "tbb8_result");
    }

    // tbb8_is_err(tbb8) -> bool
    if (callee_ident->name == "tbb8_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_is_err() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* errSentinel = getTBBErrSentinel(8);
        return builder.CreateICmpEQ(val, errSentinel, "is_err");
    }

    // tbb8_to_int(tbb8) -> int8
    if (callee_ident->name == "tbb8_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_to_int() requires one argument");
        }
        return codegenExpressionNode(expr->arguments[0].get(), this);
    }

    // ====================================================================
    // ARIA-018: TBB SENTINEL-PRESERVING WIDENING INTRINSICS
    // ====================================================================
    // These functions implement safe widening that prevents "sentinel healing"
    // where tbb8 ERR (-128) would become a valid value in tbb16 (-128 != -32768)

    // tbb16_from_tbb8(tbb8) -> tbb16
    if (callee_ident->name == "tbb16_from_tbb8") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_from_tbb8() requires one argument");
        }
        llvm::Value* srcVal = codegenExpressionNode(expr->arguments[0].get(), this);
        PrimitiveType srcType("tbb8");
        PrimitiveType dstType("tbb16");
        return tbb_codegen.generateWiden(srcVal, &srcType, &dstType);
    }

    // tbb32_from_tbb8(tbb8) -> tbb32
    if (callee_ident->name == "tbb32_from_tbb8") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_from_tbb8() requires one argument");
        }
        llvm::Value* srcVal = codegenExpressionNode(expr->arguments[0].get(), this);
        PrimitiveType srcType("tbb8");
        PrimitiveType dstType("tbb32");
        return tbb_codegen.generateWiden(srcVal, &srcType, &dstType);
    }

    // tbb32_from_tbb16(tbb16) -> tbb32
    if (callee_ident->name == "tbb32_from_tbb16") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_from_tbb16() requires one argument");
        }
        llvm::Value* srcVal = codegenExpressionNode(expr->arguments[0].get(), this);
        PrimitiveType srcType("tbb16");
        PrimitiveType dstType("tbb32");
        return tbb_codegen.generateWiden(srcVal, &srcType, &dstType);
    }

    // tbb64_from_tbb8(tbb8) -> tbb64
    if (callee_ident->name == "tbb64_from_tbb8") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_from_tbb8() requires one argument");
        }
        llvm::Value* srcVal = codegenExpressionNode(expr->arguments[0].get(), this);
        PrimitiveType srcType("tbb8");
        PrimitiveType dstType("tbb64");
        return tbb_codegen.generateWiden(srcVal, &srcType, &dstType);
    }

    // tbb64_from_tbb16(tbb16) -> tbb64
    if (callee_ident->name == "tbb64_from_tbb16") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_from_tbb16() requires one argument");
        }
        llvm::Value* srcVal = codegenExpressionNode(expr->arguments[0].get(), this);
        PrimitiveType srcType("tbb16");
        PrimitiveType dstType("tbb64");
        return tbb_codegen.generateWiden(srcVal, &srcType, &dstType);
    }

    // tbb64_from_tbb32(tbb32) -> tbb64
    if (callee_ident->name == "tbb64_from_tbb32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_from_tbb32() requires one argument");
        }
        llvm::Value* srcVal = codegenExpressionNode(expr->arguments[0].get(), this);
        PrimitiveType srcType("tbb32");
        PrimitiveType dstType("tbb64");
        return tbb_codegen.generateWiden(srcVal, &srcType, &dstType);
    }

    // tbb_widen_8_to_16(tbb8) -> tbb16 (deprecated - use tbb16_from_tbb8)
    if (callee_ident->name == "tbb_widen_8_to_16") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb_widen_8_to_16() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return generateTBBWiden(val, builder.getInt16Ty());
    }

    // tbb_widen_8_to_32(tbb8) -> tbb32
    if (callee_ident->name == "tbb_widen_8_to_32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb_widen_8_to_32() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return generateTBBWiden(val, builder.getInt32Ty());
    }

    // tbb_widen_8_to_64(tbb8) -> tbb64
    if (callee_ident->name == "tbb_widen_8_to_64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb_widen_8_to_64() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return generateTBBWiden(val, builder.getInt64Ty());
    }

    // tbb_widen_16_to_32(tbb16) -> tbb32
    if (callee_ident->name == "tbb_widen_16_to_32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb_widen_16_to_32() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return generateTBBWiden(val, builder.getInt32Ty());
    }

    // tbb_widen_16_to_64(tbb16) -> tbb64
    if (callee_ident->name == "tbb_widen_16_to_64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb_widen_16_to_64() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return generateTBBWiden(val, builder.getInt64Ty());
    }

    // tbb_widen_32_to_64(tbb32) -> tbb64
    if (callee_ident->name == "tbb_widen_32_to_64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb_widen_32_to_64() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return generateTBBWiden(val, builder.getInt64Ty());
    }

    // ====================================================================
    // TBB ARITHMETIC OPERATIONS
    // ====================================================================

    // Helper lambda to call TBB runtime functions
    auto callTBBRuntime = [&](const std::string& func_name, llvm::Type* tbbType, 
                              const std::vector<llvm::Value*>& args) -> llvm::Value* {
        llvm::Function* func = module->getFunction(func_name);
        if (!func) {
            std::vector<llvm::Type*> paramTypes;
            for (auto* arg : args) {
                paramTypes.push_back(arg->getType());
            }
            llvm::FunctionType* funcType = llvm::FunctionType::get(tbbType, paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, func_name, module);
        }
        return builder.CreateCall(func, args, "tbb_result");
    };

    // tbb64 arithmetic operations
    if (callee_ident->name == "tbb64_add") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb64_add() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb64_add", builder.getInt64Ty(), {a, b});
    }

    if (callee_ident->name == "tbb64_sub") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb64_sub() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb64_sub", builder.getInt64Ty(), {a, b});
    }

    if (callee_ident->name == "tbb64_mul") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb64_mul() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb64_mul", builder.getInt64Ty(), {a, b});
    }

    if (callee_ident->name == "tbb64_div") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb64_div() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb64_div", builder.getInt64Ty(), {a, b});
    }

    if (callee_ident->name == "tbb64_neg") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_neg() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb64_neg", builder.getInt64Ty(), {a});
    }

    if (callee_ident->name == "tbb64_abs") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_abs() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb64_abs", builder.getInt64Ty(), {a});
    }

    // tbb32 arithmetic operations
    if (callee_ident->name == "tbb32_add") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb32_add() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb32_add", builder.getInt32Ty(), {a, b});
    }

    if (callee_ident->name == "tbb32_sub") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb32_sub() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb32_sub", builder.getInt32Ty(), {a, b});
    }

    if (callee_ident->name == "tbb32_mul") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb32_mul() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb32_mul", builder.getInt32Ty(), {a, b});
    }

    if (callee_ident->name == "tbb32_div") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb32_div() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb32_div", builder.getInt32Ty(), {a, b});
    }

    if (callee_ident->name == "tbb32_neg") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_neg() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb32_neg", builder.getInt32Ty(), {a});
    }

    if (callee_ident->name == "tbb32_abs") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_abs() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb32_abs", builder.getInt32Ty(), {a});
    }

    // tbb16 arithmetic operations
    if (callee_ident->name == "tbb16_add") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb16_add() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb16_add", builder.getInt16Ty(), {a, b});
    }

    if (callee_ident->name == "tbb16_sub") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb16_sub() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb16_sub", builder.getInt16Ty(), {a, b});
    }

    if (callee_ident->name == "tbb16_mul") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb16_mul() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb16_mul", builder.getInt16Ty(), {a, b});
    }

    if (callee_ident->name == "tbb16_div") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb16_div() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb16_div", builder.getInt16Ty(), {a, b});
    }

    if (callee_ident->name == "tbb16_neg") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_neg() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb16_neg", builder.getInt16Ty(), {a});
    }

    if (callee_ident->name == "tbb16_abs") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_abs() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb16_abs", builder.getInt16Ty(), {a});
    }

    // tbb8 arithmetic operations
    if (callee_ident->name == "tbb8_add") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb8_add() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb8_add", builder.getInt8Ty(), {a, b});
    }

    if (callee_ident->name == "tbb8_sub") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb8_sub() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb8_sub", builder.getInt8Ty(), {a, b});
    }

    if (callee_ident->name == "tbb8_mul") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb8_mul() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb8_mul", builder.getInt8Ty(), {a, b});
    }

    if (callee_ident->name == "tbb8_div") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb8_div() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb8_div", builder.getInt8Ty(), {a, b});
    }

    if (callee_ident->name == "tbb8_neg") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_neg() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb8_neg", builder.getInt8Ty(), {a});
    }

    if (callee_ident->name == "tbb8_abs") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_abs() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb8_abs", builder.getInt8Ty(), {a});
    }

    // ====================================================================
    // EXOTIC TYPE CONSTRUCTORS (LBIM, Fixed Point)
    // ====================================================================
    
    // uint1024_from_int(int64) -> uint1024
    if (callee_ident->name == "uint1024_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("uint1024_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Get uint1024 struct type: { [16 x i64] }
        llvm::Type* i64Type = builder.getInt64Ty();
        llvm::ArrayType* limbsArray = llvm::ArrayType::get(i64Type, 16);
        llvm::StructType* uint1024Type = llvm::StructType::get(context, {limbsArray}, false);
        
        // Create zero-initialized struct
        llvm::Value* result = llvm::ConstantAggregateZero::get(uint1024Type);
        
        // Set first limb to input value (little-endian)
        llvm::Value* firstLimb = builder.CreateZExtOrTrunc(val, i64Type);
        result = builder.CreateInsertValue(result, firstLimb, {0, 0});
        
        return result;
    }
    
    // int1024_from_int(int64) -> int1024
    if (callee_ident->name == "int1024_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("int1024_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // int1024 is { [16 x i64] } - signed
        llvm::Type* i64Type = builder.getInt64Ty();
        llvm::ArrayType* limbsArray = llvm::ArrayType::get(i64Type, 16);
        llvm::StructType* int1024Type = llvm::StructType::get(context, {limbsArray}, false);
        
        // Sign-extend to first limb
        llvm::Value* signExtended = builder.CreateSExtOrTrunc(val, i64Type);
        
        // For negative numbers, fill high limbs with -1, otherwise 0
        llvm::Value* isNegative = builder.CreateICmpSLT(signExtended, 
            llvm::ConstantInt::get(i64Type, 0));
        llvm::Value* fillValue = builder.CreateSelect(isNegative,
            llvm::ConstantInt::get(i64Type, -1),
            llvm::ConstantInt::get(i64Type, 0));
        
        // Create result with all limbs set to fill value
        llvm::Value* result = llvm::UndefValue::get(int1024Type);
        for (unsigned i = 0; i < 16; i++) {
            llvm::Value* limbVal = (i == 0) ? signExtended : fillValue;
            result = builder.CreateInsertValue(result, limbVal, {0, i});
        }
        
        return result;
    }
    
    // ====================================================================
    // LBIM EXPONENTIATION — int*_pow(base, exp_int64) -> int*
    // Binary exponentiation via runtime: aria_lbim_pow{128,256,512,1024}
    // C ABI: struct-return-by-value, struct base by-value, uint64_t exp
    // LLVM lowering: sret + byval for large structs
    // ====================================================================
    
    if (callee_ident->name == "int128_pow" || callee_ident->name == "uint128_pow" ||
        callee_ident->name == "int256_pow" || callee_ident->name == "uint256_pow" ||
        callee_ident->name == "int512_pow" || callee_ident->name == "uint512_pow" ||
        callee_ident->name == "int1024_pow" || callee_ident->name == "uint1024_pow") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error(callee_ident->name + "() requires two arguments (base, exponent)");
        }
        
        llvm::Value* base = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* exp_val = codegenExpressionNode(expr->arguments[1].get(), this);
        
        // Determine LBIM size
        std::string funcBaseName = callee_ident->name;
        std::string typeName = funcBaseName.substr(0, funcBaseName.find("_pow"));
        unsigned numLimbs;
        std::string runtimeFunc;
        if (typeName == "int128" || typeName == "uint128") {
            numLimbs = 2; runtimeFunc = "aria_lbim_pow128";
        } else if (typeName == "int256" || typeName == "uint256") {
            numLimbs = 4; runtimeFunc = "aria_lbim_pow256";
        } else if (typeName == "int512" || typeName == "uint512") {
            numLimbs = 8; runtimeFunc = "aria_lbim_pow512";
        } else {
            numLimbs = 16; runtimeFunc = "aria_lbim_pow1024";
        }
        
        // Get struct type: { [N x i64] }
        llvm::Type* i64Type = builder.getInt64Ty();
        llvm::ArrayType* limbsArray = llvm::ArrayType::get(i64Type, numLimbs);
        llvm::StructType* structType = llvm::StructType::get(context, {limbsArray}, false);
        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
        
        // Promote base to struct if it's a scalar (e.g., int1024_pow(3, 20))
        if (!base->getType()->isStructTy()) {
            llvm::Value* signExtended = builder.CreateSExtOrTrunc(base, i64Type);
            llvm::Value* isNegative = builder.CreateICmpSLT(signExtended,
                llvm::ConstantInt::get(i64Type, 0));
            llvm::Value* fillValue = builder.CreateSelect(isNegative,
                llvm::ConstantInt::get(i64Type, -1),
                llvm::ConstantInt::get(i64Type, 0));
            llvm::Value* promoted = llvm::UndefValue::get(structType);
            for (unsigned i = 0; i < numLimbs; i++) {
                llvm::Value* limbVal = (i == 0) ? signExtended : fillValue;
                promoted = builder.CreateInsertValue(promoted, limbVal, {0, i});
            }
            base = promoted;
        }
        
        // Ensure exp is i64
        exp_val = builder.CreateZExtOrTrunc(exp_val, i64Type);
        
        // C ABI: void aria_lbim_pow*(result*, base*, uint64_t exp)
        // Using sret + byval pattern matching generateLBIMDiv
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            builder.getVoidTy(), {ptrType, ptrType, i64Type}, false);
        
        llvm::Function* func = module->getFunction(runtimeFunc);
        if (!func) {
            func = llvm::Function::Create(
                funcType, llvm::Function::ExternalLinkage, runtimeFunc, module);
            func->addParamAttr(0, llvm::Attribute::getWithStructRetType(context, structType));
            func->addParamAttr(0, llvm::Attribute::getWithAlignment(context, llvm::Align(8)));
            func->addParamAttr(1, llvm::Attribute::getWithByValType(context, structType));
            func->addParamAttr(1, llvm::Attribute::getWithAlignment(context, llvm::Align(8)));
        }
        
        llvm::Value* resultAlloca = builder.CreateAlloca(structType, nullptr, "pow_result");
        llvm::Value* baseAlloca = builder.CreateAlloca(structType, nullptr, "pow_base");
        builder.CreateStore(base, baseAlloca);
        
        builder.CreateCall(func, {resultAlloca, baseAlloca, exp_val});
        return builder.CreateLoad(structType, resultAlloca, typeName + "_pow_val");
    }
    
    // fix256_from_int(int64) -> fix256
    // ARIA-025: Create deterministic fixed-point from integer
    if (callee_ident->name == "fix256_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("fix256_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Get THE SAME fix256 struct type used in getLLVMTypeFromString
        // CRITICAL: Must use named type "struct.fix256", not anonymous struct
        // Otherwise LLVM treats them as incompatible types!
        llvm::StructType* fix256Type = llvm::StructType::getTypeByName(context, "struct.fix256");
        if (!fix256Type) {
            llvm::Type* i64Type = builder.getInt64Ty();
            llvm::ArrayType* limbsArray = llvm::ArrayType::get(i64Type, 4);
            fix256Type = llvm::StructType::create(context, {limbsArray}, "struct.fix256");
        }
        
        // SysV x86-64 ABI: fix256 is 32 bytes (>16), so returned via hidden sret pointer.
        // C ABI: void aria_fix256_from_i64(fix256* sret, int64_t)
        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(context),
            {ptrType, builder.getInt64Ty()},
            false
        );
        
        llvm::Function* runtimeFunc = module->getFunction("aria_fix256_from_i64");
        if (!runtimeFunc) {
            runtimeFunc = llvm::Function::Create(
                funcType,
                llvm::Function::ExternalLinkage,
                "aria_fix256_from_i64",
                module
            );
            runtimeFunc->addParamAttr(0, llvm::Attribute::getWithStructRetType(context, fix256Type));
        }
        
        // Convert argument to i64 if needed
        llvm::Value* i64Val = builder.CreateSExtOrTrunc(val, builder.getInt64Ty());
        
        // Allocate space for result, call with sret, load result
        llvm::Value* resultAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256_sret");
        auto* call = builder.CreateCall(runtimeFunc, {resultAlloca, i64Val});
        call->addParamAttr(0, llvm::Attribute::getWithStructRetType(context, fix256Type));
        
        return builder.CreateLoad(fix256Type, resultAlloca, "fix256_from_int_result");
    }
    
    // fix256_from_float(flt64) -> fix256
    // ARIA-025: Create deterministic fixed-point from float (APPROXIMATE - for I/O only!)
    if (callee_ident->name == "fix256_from_float") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("fix256_from_float() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Get THE SAME fix256 struct type used in getLLVMTypeFromString
        llvm::StructType* fix256Type = llvm::StructType::getTypeByName(context, "struct.fix256");
        if (!fix256Type) {
            llvm::Type* i64Type = builder.getInt64Ty();
            llvm::ArrayType* limbsArray = llvm::ArrayType::get(i64Type, 4);
            fix256Type = llvm::StructType::create(context, {limbsArray}, "struct.fix256");
        }
        
        // SysV x86-64 ABI: fix256 is 32 bytes (>16), so returned via hidden sret pointer.
        // C ABI: void aria_fix256_from_f64(fix256* sret, double)
        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(context),
            {ptrType, builder.getDoubleTy()},
            false
        );
        
        llvm::Function* runtimeFunc = module->getFunction("aria_fix256_from_f64");
        if (!runtimeFunc) {
            runtimeFunc = llvm::Function::Create(
                funcType,
                llvm::Function::ExternalLinkage,
                "aria_fix256_from_f64",
                module
            );
            runtimeFunc->addParamAttr(0, llvm::Attribute::getWithStructRetType(context, fix256Type));
        }
        
        // Convert argument to double if needed
        llvm::Value* doubleVal = val;
        if (val->getType()->isFloatTy()) {
            doubleVal = builder.CreateFPExt(val, builder.getDoubleTy());
        }
        
        // Allocate space for result, call with sret, load result
        llvm::Value* resultAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256_sret");
        auto* call = builder.CreateCall(runtimeFunc, {resultAlloca, doubleVal});
        call->addParamAttr(0, llvm::Attribute::getWithStructRetType(context, fix256Type));
        
        return builder.CreateLoad(fix256Type, resultAlloca, "fix256_from_float_result");
    }
    
    // fix256_to_int(fix256) -> int64
    // ARIA-025: Extract integer part from deterministic fixed-point
    if (callee_ident->name == "fix256_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("fix256_to_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Get THE SAME fix256 struct type used in getLLVMTypeFromString
        llvm::StructType* fix256Type = llvm::StructType::getTypeByName(context, "struct.fix256");
        if (!fix256Type) {
            llvm::Type* i64Type = builder.getInt64Ty();
            llvm::ArrayType* limbsArray = llvm::ArrayType::get(i64Type, 4);
            fix256Type = llvm::StructType::create(context, {limbsArray}, "struct.fix256");
        }
        
        // SysV x86-64 ABI: fix256 is 32 bytes (>16), so passed via byval pointer.
        // C ABI: int64_t aria_fix256_to_i64(const fix256* byval)
        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            builder.getInt64Ty(),
            {ptrType},
            false
        );
        
        llvm::Function* runtimeFunc = module->getFunction("aria_fix256_to_i64");
        if (!runtimeFunc) {
            runtimeFunc = llvm::Function::Create(
                funcType,
                llvm::Function::ExternalLinkage,
                "aria_fix256_to_i64",
                module
            );
            runtimeFunc->addParamAttr(0, llvm::Attribute::getWithByValType(context, fix256Type));
        }
        
        // Alloca + store, then pass as byval pointer
        llvm::Value* valAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256_byval");
        builder.CreateStore(val, valAlloca);
        auto* call = builder.CreateCall(runtimeFunc, {valAlloca}, "fix256_to_int_result");
        call->addParamAttr(0, llvm::Attribute::getWithByValType(context, fix256Type));
        
        return call;
    }
    
    // fix256_to_float(fix256) -> flt64
    // ARIA-025: Convert fixed-point to float (APPROXIMATE - for display only!)
    if (callee_ident->name == "fix256_to_float") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("fix256_to_float() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Get THE SAME fix256 struct type used in getLLVMTypeFromString
        llvm::StructType* fix256Type = llvm::StructType::getTypeByName(context, "struct.fix256");
        if (!fix256Type) {
            llvm::Type* i64Type = builder.getInt64Ty();
            llvm::ArrayType* limbsArray = llvm::ArrayType::get(i64Type, 4);
            fix256Type = llvm::StructType::create(context, {limbsArray}, "struct.fix256");
        }
        
        // SysV x86-64 ABI: fix256 is 32 bytes (>16), so passed via byval pointer.
        // C ABI: double aria_fix256_to_f64(const fix256* byval)
        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            builder.getDoubleTy(),
            {ptrType},
            false
        );
        
        llvm::Function* runtimeFunc = module->getFunction("aria_fix256_to_f64");
        if (!runtimeFunc) {
            runtimeFunc = llvm::Function::Create(
                funcType,
                llvm::Function::ExternalLinkage,
                "aria_fix256_to_f64",
                module
            );
            runtimeFunc->addParamAttr(0, llvm::Attribute::getWithByValType(context, fix256Type));
        }
        
        // Alloca + store, then pass as byval pointer
        llvm::Value* valAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256_byval");
        builder.CreateStore(val, valAlloca);
        auto* call = builder.CreateCall(runtimeFunc, {valAlloca}, "fix256_to_float_result");
        call->addParamAttr(0, llvm::Attribute::getWithByValType(context, fix256Type));
        
        return call;
    }
    
    // ====================================================================
    // STATIC TYPE CONSTANTS (Type.CONSTANT syntax)
    // Parser transforms Type.MEMBER → Type_MEMBER() zero-arg call
    // ====================================================================
    
    // TBB ERR sentinels
    if (callee_ident->name == "tbb8_ERR") {
        return llvm::ConstantInt::get(builder.getInt8Ty(), static_cast<uint8_t>(-128), true);
    }
    if (callee_ident->name == "tbb16_ERR") {
        return llvm::ConstantInt::get(builder.getInt16Ty(), static_cast<uint16_t>(-32768), true);
    }
    if (callee_ident->name == "tbb32_ERR") {
        return llvm::ConstantInt::get(builder.getInt32Ty(), static_cast<uint32_t>(0x80000000), true);
    }
    if (callee_ident->name == "tbb64_ERR") {
        return llvm::ConstantInt::get(builder.getInt64Ty(), static_cast<uint64_t>(0x8000000000000000ULL), true);
    }
    
    // Balanced type ERR sentinels
    if (callee_ident->name == "trit_ERR") {
        return llvm::ConstantInt::get(builder.getInt8Ty(), static_cast<uint8_t>(-128), true);
    }
    if (callee_ident->name == "nit_ERR") {
        return llvm::ConstantInt::get(builder.getInt8Ty(), static_cast<uint8_t>(-128), true);
    }
    if (callee_ident->name == "tryte_ERR") {
        return llvm::ConstantInt::get(builder.getInt16Ty(), 0xFFFF);
    }
    if (callee_ident->name == "nyte_ERR") {
        return llvm::ConstantInt::get(builder.getInt16Ty(), 0xFFFF);
    }
    
    // fix256 constants
    if (callee_ident->name == "fix256_ERR" || callee_ident->name == "fix256_MAX" || callee_ident->name == "fix256_EPSILON") {
        llvm::StructType* fix256Type = llvm::StructType::getTypeByName(context, "struct.fix256");
        if (!fix256Type) {
            llvm::Type* i64Type = builder.getInt64Ty();
            llvm::ArrayType* limbsArray = llvm::ArrayType::get(i64Type, 4);
            fix256Type = llvm::StructType::create(context, {limbsArray}, "struct.fix256");
        }
        llvm::ArrayType* limbsArray = llvm::ArrayType::get(builder.getInt64Ty(), 4);
        
        std::vector<llvm::Constant*> limbs(4);
        if (callee_ident->name == "fix256_ERR") {
            // ERR sentinel: most negative value (integer part = INT128_MIN, fraction = 0)
            limbs[0] = llvm::ConstantInt::get(builder.getInt64Ty(), 0);
            limbs[1] = llvm::ConstantInt::get(builder.getInt64Ty(), 0);
            limbs[2] = llvm::ConstantInt::get(builder.getInt64Ty(), 0);
            limbs[3] = llvm::ConstantInt::get(builder.getInt64Ty(), 0x8000000000000000ULL);
        } else if (callee_ident->name == "fix256_MAX") {
            // MAX: most positive value (integer part = INT128_MAX, fraction = all 1s)
            limbs[0] = llvm::ConstantInt::get(builder.getInt64Ty(), UINT64_MAX);
            limbs[1] = llvm::ConstantInt::get(builder.getInt64Ty(), UINT64_MAX);
            limbs[2] = llvm::ConstantInt::get(builder.getInt64Ty(), UINT64_MAX);
            limbs[3] = llvm::ConstantInt::get(builder.getInt64Ty(), 0x7FFFFFFFFFFFFFFFULL);
        } else { // fix256_EPSILON
            // EPSILON: smallest positive value (2^-128)
            limbs[0] = llvm::ConstantInt::get(builder.getInt64Ty(), 1);
            limbs[1] = llvm::ConstantInt::get(builder.getInt64Ty(), 0);
            limbs[2] = llvm::ConstantInt::get(builder.getInt64Ty(), 0);
            limbs[3] = llvm::ConstantInt::get(builder.getInt64Ty(), 0);
        }
        
        llvm::Constant* limbsConst = llvm::ConstantArray::get(limbsArray, limbs);
        return llvm::ConstantStruct::get(fix256Type, {limbsConst});
    }
    
    // trit_from_int(int64) -> trit
    if (callee_ident->name == "trit_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("trit_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        // Truncate to i8 (balanced ternary: -1, 0, 1)
        return builder.CreateTrunc(val, builder.getInt8Ty());
    }
    
    // nit_from_int(int64) -> nit
    if (callee_ident->name == "nit_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("nit_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        // Truncate to i8 (balanced nonary: -4 to 4)
        return builder.CreateTrunc(val, builder.getInt8Ty());
    }
    
    // tryte_from_int(int64) -> tryte (10 trits packed in uint16)
    if (callee_ident->name == "tryte_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tryte_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        // Truncate to i16 (proper encoding needs runtime support)
        return builder.CreateTrunc(val, builder.getInt16Ty());
    }
    
    // nyte_from_int(int64) -> nyte (5 nits packed in uint16)
    if (callee_ident->name == "nyte_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("nyte_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        // Truncate to i16 (proper encoding needs runtime support)
        return builder.CreateTrunc(val, builder.getInt16Ty());
    }
    
    // ====================================================================
    // TRIT/NIT LOGIC AND ARITHMETIC INTRINSICS
    // ====================================================================
    // Trit: Single balanced ternary digit (-1, 0, 1, ERR=-128)
    // Nit: Single balanced nonary digit (-4 to +4, ERR=-128)
    
    // trit_add(trit, trit) -> trit
    if (callee_ident->name == "trit_add") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("trit_add() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        
        llvm::Function* func = module->getFunction("trit_add");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt8Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "trit_add", module);
        }
        return builder.CreateCall(func, {a, b}, "trit_add_result");
    }
    
    // trit_mul(trit, trit) -> trit
    if (callee_ident->name == "trit_mul") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("trit_mul() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        
        llvm::Function* func = module->getFunction("trit_mul");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt8Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "trit_mul", module);
        }
        return builder.CreateCall(func, {a, b}, "trit_mul_result");
    }
    
    // trit_neg(trit) -> trit
    if (callee_ident->name == "trit_neg") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("trit_neg() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        
        llvm::Function* func = module->getFunction("trit_neg");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt8Ty(), {builder.getInt8Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "trit_neg", module);
        }
        return builder.CreateCall(func, {a}, "trit_neg_result");
    }
    
    // REMOVED: Old trit_and - now handled in TERNARY LOGIC BUILTINS section below
    
    // REMOVED: Old trit_or - now handled in TERNARY LOGIC BUILTINS section below
    
    // REMOVED: Old trit_not - now handled in TERNARY LOGIC BUILTINS section below
    
    // trit_is_err(trit) -> bool
    if (callee_ident->name == "trit_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("trit_is_err() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        
        llvm::Function* func = module->getFunction("trit_is_err");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt1Ty(), {builder.getInt8Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "trit_is_err", module);
        }
        return builder.CreateCall(func, {a}, "trit_is_err_result");
    }
    
    // REMOVED: Old nit_and/nit_or - now handled in TERNARY LOGIC BUILTINS section
    
    
    // nit_is_err(nit) -> bool
    if (callee_ident->name == "nit_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("nit_is_err() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Call aria_nit_is_err (returns i8 0/1), truncate to i1 for bool
        llvm::Function* func = module->getFunction("aria_nit_is_err");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt8Ty(), {builder.getInt8Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "aria_nit_is_err", module);
        }
        llvm::Value* result_i8 = builder.CreateCall(func, {a}, "nit_is_err_i8");
        return builder.CreateTrunc(result_i8, builder.getInt1Ty(), "nit_is_err_result");
    }
    
    // ====================================================================
    // TRIT_NOT (was removed from above, re-added here)
    // ====================================================================
    
    // trit_not(trit) -> trit
    if (callee_ident->name == "trit_not") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("trit_not() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* func = module->getFunction("trit_not");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt8Ty(), {builder.getInt8Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "trit_not", module);
        }
        return builder.CreateCall(func, {a}, "trit_not_result");
    }
    
    // ====================================================================
    // NIT ARITHMETIC OPERATIONS
    // ====================================================================
    
    // nit_add(nit, nit) -> nit
    if (callee_ident->name == "nit_add") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("nit_add() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Function* func = module->getFunction("nit_add");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt8Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nit_add", module);
        }
        return builder.CreateCall(func, {a, b}, "nit_add_result");
    }
    
    // nit_sub(nit, nit) -> nit
    if (callee_ident->name == "nit_sub") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("nit_sub() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Function* func = module->getFunction("nit_sub");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt8Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nit_sub", module);
        }
        return builder.CreateCall(func, {a, b}, "nit_sub_result");
    }
    
    // nit_mul(nit, nit) -> nit
    if (callee_ident->name == "nit_mul") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("nit_mul() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Function* func = module->getFunction("nit_mul");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt8Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nit_mul", module);
        }
        return builder.CreateCall(func, {a, b}, "nit_mul_result");
    }
    
    // nit_div(nit, nit) -> nit
    if (callee_ident->name == "nit_div") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("nit_div() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Function* func = module->getFunction("nit_div");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt8Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nit_div", module);
        }
        return builder.CreateCall(func, {a, b}, "nit_div_result");
    }
    
    // nit_neg(nit) -> nit
    if (callee_ident->name == "nit_neg") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("nit_neg() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* func = module->getFunction("nit_neg");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt8Ty(), {builder.getInt8Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nit_neg", module);
        }
        return builder.CreateCall(func, {a}, "nit_neg_result");
    }
    
    // nit_abs(nit) -> nit
    if (callee_ident->name == "nit_abs") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("nit_abs() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* func = module->getFunction("nit_abs");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt8Ty(), {builder.getInt8Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nit_abs", module);
        }
        return builder.CreateCall(func, {a}, "nit_abs_result");
    }
    
    // ====================================================================
    // TRYTE ARITHMETIC OPERATIONS (uint16 biased representation)
    // ====================================================================
    
    // tryte_add(tryte, tryte) -> tryte
    if (callee_ident->name == "tryte_add") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tryte_add() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Function* func = module->getFunction("tryte_add");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt16Ty(), builder.getInt16Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "tryte_add", module);
        }
        return builder.CreateCall(func, {a, b}, "tryte_add_result");
    }
    
    // tryte_sub(tryte, tryte) -> tryte
    if (callee_ident->name == "tryte_sub") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tryte_sub() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Function* func = module->getFunction("tryte_sub");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt16Ty(), builder.getInt16Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "tryte_sub", module);
        }
        return builder.CreateCall(func, {a, b}, "tryte_sub_result");
    }
    
    // tryte_mul(tryte, tryte) -> tryte
    if (callee_ident->name == "tryte_mul") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tryte_mul() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Function* func = module->getFunction("tryte_mul");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt16Ty(), builder.getInt16Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "tryte_mul", module);
        }
        return builder.CreateCall(func, {a, b}, "tryte_mul_result");
    }
    
    // tryte_div(tryte, tryte) -> tryte
    if (callee_ident->name == "tryte_div") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tryte_div() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Function* func = module->getFunction("tryte_div");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt16Ty(), builder.getInt16Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "tryte_div", module);
        }
        return builder.CreateCall(func, {a, b}, "tryte_div_result");
    }
    
    // tryte_mod(tryte, tryte) -> tryte
    if (callee_ident->name == "tryte_mod") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tryte_mod() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Function* func = module->getFunction("tryte_mod");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt16Ty(), builder.getInt16Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "tryte_mod", module);
        }
        return builder.CreateCall(func, {a, b}, "tryte_mod_result");
    }
    
    // tryte_neg(tryte) -> tryte
    if (callee_ident->name == "tryte_neg") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tryte_neg() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* func = module->getFunction("tryte_neg");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), {builder.getInt16Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "tryte_neg", module);
        }
        return builder.CreateCall(func, {a}, "tryte_neg_result");
    }
    
    // tryte_abs(tryte) -> tryte
    if (callee_ident->name == "tryte_abs") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tryte_abs() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* func = module->getFunction("tryte_abs");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), {builder.getInt16Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "tryte_abs", module);
        }
        return builder.CreateCall(func, {a}, "tryte_abs_result");
    }
    
    // tryte_is_err(tryte) -> bool
    if (callee_ident->name == "tryte_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tryte_is_err() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* func = module->getFunction("tryte_is_err");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt1Ty(), {builder.getInt16Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "tryte_is_err", module);
        }
        return builder.CreateCall(func, {a}, "tryte_is_err_result");
    }
    
    // ====================================================================
    // NYTE ARITHMETIC OPERATIONS (uint16 biased representation)
    // ====================================================================
    
    // nyte_add(nyte, nyte) -> nyte
    if (callee_ident->name == "nyte_add") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("nyte_add() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Function* func = module->getFunction("nyte_add");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt16Ty(), builder.getInt16Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nyte_add", module);
        }
        return builder.CreateCall(func, {a, b}, "nyte_add_result");
    }
    
    // nyte_sub(nyte, nyte) -> nyte
    if (callee_ident->name == "nyte_sub") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("nyte_sub() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Function* func = module->getFunction("nyte_sub");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt16Ty(), builder.getInt16Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nyte_sub", module);
        }
        return builder.CreateCall(func, {a, b}, "nyte_sub_result");
    }
    
    // nyte_mul(nyte, nyte) -> nyte
    if (callee_ident->name == "nyte_mul") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("nyte_mul() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Function* func = module->getFunction("nyte_mul");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt16Ty(), builder.getInt16Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nyte_mul", module);
        }
        return builder.CreateCall(func, {a, b}, "nyte_mul_result");
    }
    
    // nyte_div(nyte, nyte) -> nyte
    if (callee_ident->name == "nyte_div") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("nyte_div() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Function* func = module->getFunction("nyte_div");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt16Ty(), builder.getInt16Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nyte_div", module);
        }
        return builder.CreateCall(func, {a, b}, "nyte_div_result");
    }
    
    // nyte_mod(nyte, nyte) -> nyte
    if (callee_ident->name == "nyte_mod") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("nyte_mod() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Function* func = module->getFunction("nyte_mod");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt16Ty(), builder.getInt16Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nyte_mod", module);
        }
        return builder.CreateCall(func, {a, b}, "nyte_mod_result");
    }
    
    // nyte_neg(nyte) -> nyte
    if (callee_ident->name == "nyte_neg") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("nyte_neg() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* func = module->getFunction("nyte_neg");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), {builder.getInt16Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nyte_neg", module);
        }
        return builder.CreateCall(func, {a}, "nyte_neg_result");
    }
    
    // nyte_abs(nyte) -> nyte
    if (callee_ident->name == "nyte_abs") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("nyte_abs() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* func = module->getFunction("nyte_abs");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), {builder.getInt16Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nyte_abs", module);
        }
        return builder.CreateCall(func, {a}, "nyte_abs_result");
    }
    
    // nyte_is_err(nyte) -> bool
    if (callee_ident->name == "nyte_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("nyte_is_err() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* func = module->getFunction("nyte_is_err");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt1Ty(), {builder.getInt16Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nyte_is_err", module);
        }
        return builder.CreateCall(func, {a}, "nyte_is_err_result");
    }
    
    // ====================================================================
    // TRYTE/NYTE CONVERSION OPERATIONS
    // ====================================================================
    
    // tryte_from_balanced(int32) -> tryte
    if (callee_ident->name == "tryte_from_balanced") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tryte_from_balanced() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* func = module->getFunction("tryte_from_balanced");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), {builder.getInt32Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "tryte_from_balanced", module);
        }
        return builder.CreateCall(func, {a}, "tryte_from_balanced_result");
    }
    
    // nyte_from_balanced(int32) -> nyte
    if (callee_ident->name == "nyte_from_balanced") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("nyte_from_balanced() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* func = module->getFunction("nyte_from_balanced");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), {builder.getInt32Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nyte_from_balanced", module);
        }
        return builder.CreateCall(func, {a}, "nyte_from_balanced_result");
    }
    
    // tryte_to_balanced(tryte) -> int32
    if (callee_ident->name == "tryte_to_balanced") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tryte_to_balanced() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* func = module->getFunction("tryte_to_balanced");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt32Ty(), {builder.getInt16Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "tryte_to_balanced", module);
        }
        return builder.CreateCall(func, {a}, "tryte_to_balanced_result");
    }
    
    // nyte_to_balanced(nyte) -> int32
    if (callee_ident->name == "nyte_to_balanced") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("nyte_to_balanced() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* func = module->getFunction("nyte_to_balanced");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt32Ty(), {builder.getInt16Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nyte_to_balanced", module);
        }
        return builder.CreateCall(func, {a}, "nyte_to_balanced_result");
    }
    
    // tryte_to_nyte(tryte) -> nyte
    if (callee_ident->name == "tryte_to_nyte") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tryte_to_nyte() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* func = module->getFunction("tryte_to_nyte");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), {builder.getInt16Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "tryte_to_nyte", module);
        }
        return builder.CreateCall(func, {a}, "tryte_to_nyte_result");
    }
    
    // nyte_to_tryte(nyte) -> tryte
    if (callee_ident->name == "nyte_to_tryte") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("nyte_to_tryte() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* func = module->getFunction("nyte_to_tryte");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt16Ty(), {builder.getInt16Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nyte_to_tryte", module);
        }
        return builder.CreateCall(func, {a}, "nyte_to_tryte_result");
    }
    
    // ====================================================================
    // MATRIX AND TENSOR OPERATIONS (Exotic Types)
    // ====================================================================
    
    // tmatrix_identity(rows, cols) -> tmatrix
    if (callee_ident->name == "tmatrix_identity") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tmatrix_identity() requires two arguments (rows, cols)");
        }
        llvm::Value* rows = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* cols = codegenExpressionNode(expr->arguments[1].get(), this);
        
        // Create external function declaration for runtime implementation
        llvm::Function* func = module->getFunction("tmatrix_identity");
        if (!func) {
            // tmatrix is represented as an opaque pointer for now
            llvm::Type* tmatrixType = builder.getPtrTy();
            std::vector<llvm::Type*> paramTypes = {builder.getInt64Ty(), builder.getInt64Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(tmatrixType, paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "tmatrix_identity", module);
        }
        return builder.CreateCall(func, {rows, cols}, "tmatrix");
    }
    
    // ttensor_zeros(shape) -> ttensor
    if (callee_ident->name == "ttensor_zeros") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("ttensor_zeros() requires one argument (9D shape)");
        }
        llvm::Value* shape = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Create external function declaration for runtime implementation
        llvm::Function* func = module->getFunction("ttensor_zeros");
        if (!func) {
            // ttensor is represented as an opaque pointer for now
            llvm::Type* ttensorType = builder.getPtrTy();
            // Shape is a vec9 (9 x int64 array)
            llvm::Type* shapeType = shape->getType();
            std::vector<llvm::Type*> paramTypes = {shapeType};
            llvm::FunctionType* funcType = llvm::FunctionType::get(ttensorType, paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "ttensor_zeros", module);
        }
        return builder.CreateCall(func, {shape}, "ttensor");
    }
    
    // ====================================================================
    // MAP/DICTIONARY RUNTIME FUNCTIONS
    // ====================================================================
    
    // map_new(key_size, value_size) -> wild void->
    if (callee_ident->name == "map_new") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_new() requires 2 arguments (key_size, value_size)");
        }
        llvm::Value* key_size = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* value_size = codegenExpressionNode(expr->arguments[1].get(), this);
        
        llvm::Function* func = module->getFunction("map_new");
        if (!func) {
            llvm::Type* ptrType = builder.getPtrTy();  // Returns opaque pointer
            std::vector<llvm::Type*> paramTypes = {builder.getInt64Ty(), builder.getInt64Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(ptrType, paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "map_new", module);
        }
        // Widen arguments to i64 if needed (literals default to i32)
        if (key_size->getType() != builder.getInt64Ty() && key_size->getType()->isIntegerTy())
            key_size = builder.CreateSExt(key_size, builder.getInt64Ty(), "map_key_sz");
        if (value_size->getType() != builder.getInt64Ty() && value_size->getType()->isIntegerTy())
            value_size = builder.CreateSExt(value_size, builder.getInt64Ty(), "map_val_sz");
        return builder.CreateCall(func, {key_size, value_size}, "map");
    }
    
    // map_insert(map, key_ptr, value_ptr) -> void
    if (callee_ident->name == "map_insert") {
        if (expr->arguments.size() != 3) {
            throw std::runtime_error("map_insert() requires 3 arguments (map, key_ptr, value_ptr)");
        }
        llvm::Value* map = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* value_ptr = codegenExpressionNode(expr->arguments[2].get(), this);
        
        llvm::Function* func = module->getFunction("map_insert");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {
                builder.getPtrTy(), 
                builder.getPtrTy(), 
                builder.getPtrTy()
            };
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getVoidTy(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "map_insert", module);
        }
        return builder.CreateCall(func, {map, key_ptr, value_ptr});
    }
    
    // map_get(map, key_ptr) -> wild void->
    if (callee_ident->name == "map_get") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_get() requires 2 arguments (map, key_ptr)");
        }
        llvm::Value* map = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        
        llvm::Function* func = module->getFunction("map_get");
        if (!func) {
            llvm::Type* ptrType = builder.getPtrTy();
            std::vector<llvm::Type*> paramTypes = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(ptrType, paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "map_get", module);
        }
        return builder.CreateCall(func, {map, key_ptr}, "value_ptr");
    }
    
    // map_has(map, key_ptr) -> bool
    if (callee_ident->name == "map_has") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_has() requires 2 arguments (map, key_ptr)");
        }
        llvm::Value* map = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        
        llvm::Function* func = module->getFunction("map_has");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt1Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "map_has", module);
        }
        return builder.CreateCall(func, {map, key_ptr}, "has_key");
    }
    
    // map_remove(map, key_ptr) -> void
    if (callee_ident->name == "map_remove") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_remove() requires 2 arguments (map, key_ptr)");
        }
        llvm::Value* map = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        
        llvm::Function* func = module->getFunction("map_remove");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getVoidTy(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "map_remove", module);
        }
        return builder.CreateCall(func, {map, key_ptr});
    }
    
    // map_length(map) -> int64
    if (callee_ident->name == "map_length") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("map_length() requires 1 argument (map)");
        }
        llvm::Value* map = codegenExpressionNode(expr->arguments[0].get(), this);
        
        llvm::Function* func = module->getFunction("map_length");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getPtrTy()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt64Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "map_length", module);
        }
        return builder.CreateCall(func, {map}, "length");
    }
    
    // ====================================================================
    // OPTIONAL/RESULT HELPER FUNCTIONS
    // ====================================================================
    
    // get_optional_value(condition) -> int64?
    if (callee_ident->name == "get_optional_value") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("get_optional_value() requires 1 argument (condition)");
        }
        llvm::Value* condition = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Convert condition to i1 if needed
        if (condition->getType() != builder.getInt1Ty()) {
            if (condition->getType()->isIntegerTy()) {
                condition = builder.CreateICmpNE(condition, 
                    llvm::ConstantInt::get(condition->getType(), 0), "tobool");
            }
        }
        
        // Optional<int64> is { i1 hasValue, i64 value }
        llvm::StructType* optionalType = llvm::StructType::get(context, 
            {builder.getInt1Ty(), builder.getInt64Ty()}, false);
        
        llvm::Value* result = llvm::UndefValue::get(optionalType);
        
        // Set hasValue based on condition
        result = builder.CreateInsertValue(result, condition, 0, "optional.hasValue");
        
        // Set value to 123 if condition is true, otherwise 0
        llvm::Value* valueToStore = builder.CreateSelect(condition,
            llvm::ConstantInt::get(builder.getInt64Ty(), 123),
            llvm::ConstantInt::get(builder.getInt64Ty(), 0));
        result = builder.CreateInsertValue(result, valueToStore, 1, "optional.value");
        
        return result;
    }
    
    // ====================================================================
    // INTEGER CONVERSION BUILTINS
    // ====================================================================
    
    // int8_from_int64(int64) -> int8
    if (callee_ident->name == "int8_from_int64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("int8_from_int64() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateTrunc(val, builder.getInt8Ty());
    }
    
    // int16_from_int64(int64) -> int16
    if (callee_ident->name == "int16_from_int64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("int16_from_int64() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateTrunc(val, builder.getInt16Ty());
    }
    
    // int32_from_int64(int64) -> int32
    if (callee_ident->name == "int32_from_int64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("int32_from_int64() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateTrunc(val, builder.getInt32Ty());
    }
    
    // int64_from_int32(int32) -> int64
    if (callee_ident->name == "int64_from_int32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("int64_from_int32() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateSExt(val, builder.getInt64Ty());
    }
    
    // int64_from_int8(int8) -> int64
    if (callee_ident->name == "int64_from_int8") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("int64_from_int8() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateSExt(val, builder.getInt64Ty());
    }
    
    // ====================================================================
    // MEMORY MANAGEMENT BUILTINS
    // ====================================================================
    
    // ====================================================================
    // EXPLICIT SAFETY BYPASS
    // ====================================================================

    // raw(Result<T>) -> T — Bypass "no checky no val", extract .value directly.
    // Type checker has already confirmed the argument is Result<T>.
    // We extract field 0 of the {T, ptr, i8} Result struct without any guard.
    if (callee_ident->name == "raw") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("raw() requires exactly one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg) {
            throw std::runtime_error("Failed to generate code for raw() argument");
        }
        // arg is a Result<T> struct: { T value, ptr error, i8 is_error }
        // Extract field 0 (value) unconditionally.
        if (arg->getType()->isStructTy()) {
            llvm::StructType* sty = llvm::cast<llvm::StructType>(arg->getType());
            if (sty->getNumElements() == 3 &&
                sty->getElementType(1)->isPointerTy() &&
                sty->getElementType(2)->isIntegerTy(8)) {
                return builder.CreateExtractValue(arg, 0, "raw.value");
            }
        }
        // If for some reason it arrived already unwrapped (e.g. constant folding),
        // pass it straight through.
        return arg;
    }

    // drop(T) -> void - Evaluate expression and discard result
    if (callee_ident->name == "drop") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("drop() requires one argument");
        }
        
        // Evaluate the expression (for side effects) and discard the result
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // If the dropped expression is a call to an async function, resume and free
        // the coroutine so its body actually executes
        if (val && val->getType()->isPointerTy()) {
            ASTNode* arg = expr->arguments[0].get();
            if (arg->type == ASTNode::NodeType::CALL) {
                CallExpr* innerCall = static_cast<CallExpr*>(arg);
                if (innerCall->callee->type == ASTNode::NodeType::IDENTIFIER) {
                    IdentifierExpr* innerIdent = static_cast<IdentifierExpr*>(innerCall->callee.get());
                    llvm::Function* callee_fn = module->getFunction(innerIdent->name);
                    if (callee_fn && callee_fn->hasMetadata("aria.async")) {
                        // Resume the coroutine (runs the body to completion)
                        llvm::Function* coro_resume = llvm::Intrinsic::getOrInsertDeclaration(
                            module, llvm::Intrinsic::coro_resume);
                        builder.CreateCall(coro_resume, {val});
                        // Destroy the coroutine frame (triggers cleanup → free)
                        llvm::Function* coro_destroy = llvm::Intrinsic::getOrInsertDeclaration(
                            module, llvm::Intrinsic::coro_destroy);
                        builder.CreateCall(coro_destroy, {val});
                    }
                }
            }
        }
        
        // drop() returns void - LLVM represents this as no value
        return nullptr;
    }

    // sleep_ms(int64) -> void - Sleep for given milliseconds
    if (callee_ident->name == "sleep_ms") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("sleep_ms() requires one argument");
        }
        llvm::Value* ms_val = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!ms_val) {
            throw std::runtime_error("Failed to generate code for sleep_ms argument");
        }
        if (ms_val->getType() != builder.getInt64Ty()) {
            ms_val = builder.CreateIntCast(ms_val, builder.getInt64Ty(), true);
        }
        llvm::Function* sleep_fn = module->getFunction("aria_sleep_ms");
        if (!sleep_fn) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt64Ty()}, false);
            sleep_fn = llvm::Function::Create(
                ft, llvm::Function::ExternalLinkage, "aria_sleep_ms", module);
        }
        builder.CreateCall(sleep_fn, {ms_val});
        return nullptr;
    }

    // exit(int32) -> noreturn — terminate the process with the given exit code.
    // Only valid in main/failsafe — reject in lambdas and regular functions.
    if (callee_ident->name == "exit") {
        llvm::Function* parentFn = builder.GetInsertBlock()->getParent();
        llvm::StringRef fnName = parentFn->getName();
        if (fnName != "main" && fnName != "failsafe") {
            throw std::runtime_error(
                "'exit' can only be used in 'main' or 'failsafe'. "
                "Use 'pass'/'fail' to return from functions and lambdas. "
                "(found in function '" + fnName.str() + "')");
        }
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("exit() requires one argument (exit code: int32)");
        }
        llvm::Value* code_val = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!code_val) {
            throw std::runtime_error("Failed to generate code for exit() argument");
        }
        if (code_val->getType() != builder.getInt32Ty()) {
            code_val = builder.CreateIntCast(code_val, builder.getInt32Ty(), true);
        }
        llvm::Function* exit_fn = module->getFunction("exit");
        if (!exit_fn) {
            llvm::FunctionType* ft = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt32Ty()}, false);
            exit_fn = llvm::Function::Create(
                ft, llvm::Function::ExternalLinkage, "exit", module);
            exit_fn->addFnAttr(llvm::Attribute::NoReturn);
        }
        llvm::CallInst* call = builder.CreateCall(exit_fn, {code_val});
        call->setDoesNotReturn();
        builder.CreateUnreachable();
        return nullptr;
    }

    // env_get(string) -> string — return the value of an environment variable,
    // or an empty string if the variable is not set.
    if (callee_ident->name == "env_get") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("env_get() requires one argument (name: string)");
        }
        llvm::Function* func = module->getFunction("aria_env_get_builtin");
        if (!func) {
            // aria_env_get_builtin(AriaString name) -> AriaString*
            std::vector<llvm::Type*> params = {getAriaStringType()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_env_get_builtin", module);
        }
        llvm::Value* name_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* name_val = builder.CreateLoad(getAriaStringType(), name_ptr, "env_name");
        return builder.CreateCall(func, {name_val}, "env_val");
    }

    // sort_lines(string) -> string — sort newline-separated lines lexicographically.
    if (callee_ident->name == "sort_lines") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("sort_lines() requires one argument (content: string)");
        }
        llvm::Function* func = module->getFunction("aria_sort_lines");
        if (!func) {
            // aria_sort_lines(AriaString* content) -> AriaString*
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(getAriaStringType(), 0)
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_sort_lines", module);
        }
        llvm::Value* content_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {content_ptr}, "sorted_lines");
    }

    // ====================================================================
    // COLLECTION BUILTINS (Phase 6.2)
    // ====================================================================

    // array_new(element_size: int64) -> ptr
    if (callee_ident->name == "array_new") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("array_new() requires one argument (element_size)");
        }

        // Get or declare aria_array_new_simple
        llvm::Function* func = module->getFunction("aria_array_new_simple");
        if (!func) {
            // AriaArray* aria_array_new_simple(size_t element_size, int type_id)
            std::vector<llvm::Type*> params = {
                builder.getInt64Ty(),  // element_size (size_t)
                builder.getInt32Ty()   // type_id
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_array_new_simple", module);
        }

        llvm::Value* element_size = codegenExpressionNode(expr->arguments[0].get(), this);
        // Ensure it's i64
        if (element_size->getType()->getIntegerBitWidth() != 64) {
            element_size = builder.CreateZExtOrTrunc(element_size, builder.getInt64Ty());
        }
        llvm::Value* type_id = llvm::ConstantInt::get(builder.getInt32Ty(), 0); // Generic type

        return builder.CreateCall(func, {element_size, type_id}, "new_array");
    }

    // array_length(array: ptr) -> int64
    if (callee_ident->name == "array_length") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("array_length() requires one argument (array)");
        }

        // Get or declare aria_array_length
        llvm::Function* func = module->getFunction("aria_array_length");
        if (!func) {
            // size_t aria_array_length(const AriaArray* array)
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_array_length", module);
        }

        llvm::Value* array_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {array_ptr}, "array_len");
    }

    // array_push(array: ptr, value: ptr) -> void
    if (callee_ident->name == "array_push") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("array_push() requires two arguments (array, value)");
        }

        // Get or declare aria_array_push_simple
        llvm::Function* func = module->getFunction("aria_array_push_simple");
        if (!func) {
            // void aria_array_push_simple(AriaArray* array, const void* value)
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getVoidTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_array_push_simple", module);
        }

        llvm::Value* array_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* value_ptr = codegenExpressionNode(expr->arguments[1].get(), this);

        // If value is not already a pointer, we need to allocate and store it
        if (!value_ptr->getType()->isPointerTy()) {
            llvm::Value* alloca = builder.CreateAlloca(value_ptr->getType(), nullptr, "push_temp");
            builder.CreateStore(value_ptr, alloca);
            value_ptr = alloca;
        }

        builder.CreateCall(func, {array_ptr, value_ptr});
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);  // Return 0 on success
    }

    // array_get(array: ptr, index: int64) -> ptr
    if (callee_ident->name == "array_get") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("array_get() requires two arguments (array, index)");
        }

        // Get or declare aria_array_get_simple
        llvm::Function* func = module->getFunction("aria_array_get_simple");
        if (!func) {
            // void* aria_array_get_simple(AriaArray* array, size_t index)
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getInt64Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_array_get_simple", module);
        }

        llvm::Value* array_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* index = codegenExpressionNode(expr->arguments[1].get(), this);
        // Ensure index is i64
        if (index->getType()->getIntegerBitWidth() != 64) {
            index = builder.CreateZExtOrTrunc(index, builder.getInt64Ty());
        }

        return builder.CreateCall(func, {array_ptr, index}, "array_elem");
    }

    // array_set(array: ptr, index: int64, value: ptr) -> void
    if (callee_ident->name == "array_set") {
        if (expr->arguments.size() != 3) {
            throw std::runtime_error("array_set() requires three arguments (array, index, value)");
        }

        // Get or declare aria_array_set_simple
        llvm::Function* func = module->getFunction("aria_array_set_simple");
        if (!func) {
            // void aria_array_set_simple(AriaArray* array, size_t index, const void* value)
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getInt64Ty(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getVoidTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_array_set_simple", module);
        }

        llvm::Value* array_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* index = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* value_ptr = codegenExpressionNode(expr->arguments[2].get(), this);

        // Ensure index is i64
        if (index->getType()->getIntegerBitWidth() != 64) {
            index = builder.CreateZExtOrTrunc(index, builder.getInt64Ty());
        }

        // If value is not already a pointer, we need to allocate and store it
        if (!value_ptr->getType()->isPointerTy()) {
            llvm::Value* alloca = builder.CreateAlloca(value_ptr->getType(), nullptr, "set_temp");
            builder.CreateStore(value_ptr, alloca);
            value_ptr = alloca;
        }

        builder.CreateCall(func, {array_ptr, index, value_ptr});
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);  // Return 0 on success
    }

    // array_pop(array: ptr) -> ptr
    if (callee_ident->name == "array_pop") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("array_pop() requires one argument (array)");
        }

        // Get or declare aria_array_pop_simple
        llvm::Function* func = module->getFunction("aria_array_pop_simple");
        if (!func) {
            // void aria_array_pop_simple(AriaArray* array, void* out_value)
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getVoidTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_array_pop_simple", module);
        }

        llvm::Value* array_ptr = codegenExpressionNode(expr->arguments[0].get(), this);

        // Allocate space for the popped value (we'll use an i64-sized buffer)
        llvm::Value* out_alloca = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "pop_out");

        builder.CreateCall(func, {array_ptr, out_alloca});

        // Return the pointer to the buffer (caller should cast/load as appropriate)
        return out_alloca;
    }

    // ════════════════════════════════════════════════════════════════════
    // Map Functions
    // ════════════════════════════════════════════════════════════════════

    // map_new(key_size: int64, value_size: int64) -> ptr
    if (callee_ident->name == "map_new") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_new() requires two arguments (key_size, value_size)");
        }

        // Get or declare aria_map_new_simple
        llvm::Function* func = module->getFunction("aria_map_new_simple");
        if (!func) {
            // AriaMap* aria_map_new_simple(size_t key_size, size_t value_size)
            std::vector<llvm::Type*> params = {builder.getInt64Ty(), builder.getInt64Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_map_new_simple", module);
        }

        llvm::Value* key_size = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* value_size = codegenExpressionNode(expr->arguments[1].get(), this);
        
        // Ensure they're i64
        if (key_size->getType()->getIntegerBitWidth() != 64) {
            key_size = builder.CreateZExtOrTrunc(key_size, builder.getInt64Ty());
        }
        if (value_size->getType()->getIntegerBitWidth() != 64) {
            value_size = builder.CreateZExtOrTrunc(value_size, builder.getInt64Ty());
        }

        return builder.CreateCall(func, {key_size, value_size}, "new_map");
    }

    // map_length(map: ptr) -> int64
    if (callee_ident->name == "map_length") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("map_length() requires one argument (map)");
        }

        // Get or declare aria_map_length
        llvm::Function* func = module->getFunction("aria_map_length");
        if (!func) {
            // size_t aria_map_length(const AriaMap* map)
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_map_length", module);
        }

        llvm::Value* map_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {map_ptr}, "map_len");
    }

    // map_insert(map: ptr, key: ptr, value: ptr) -> void
    if (callee_ident->name == "map_insert") {
        if (expr->arguments.size() != 3) {
            throw std::runtime_error("map_insert() requires three arguments (map, key, value)");
        }

        // Get or declare aria_map_insert_simple
        llvm::Function* func = module->getFunction("aria_map_insert_simple");
        if (!func) {
            // void aria_map_insert_simple(AriaMap* map, const void* key, const void* value)
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getVoidTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_map_insert_simple", module);
        }

        llvm::Value* map_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* value_ptr = codegenExpressionNode(expr->arguments[2].get(), this);

        // If key is not already a pointer, allocate and store it
        if (!key_ptr->getType()->isPointerTy()) {
            llvm::Value* alloca = builder.CreateAlloca(key_ptr->getType(), nullptr, "key_temp");
            builder.CreateStore(key_ptr, alloca);
            key_ptr = alloca;
        }

        // If value is not already a pointer, allocate and store it
        if (!value_ptr->getType()->isPointerTy()) {
            llvm::Value* alloca = builder.CreateAlloca(value_ptr->getType(), nullptr, "value_temp");
            builder.CreateStore(value_ptr, alloca);
            value_ptr = alloca;
        }

        builder.CreateCall(func, {map_ptr, key_ptr, value_ptr});
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);  // Return 0 on success
    }

    // map_get(map: ptr, key: ptr) -> ptr
    if (callee_ident->name == "map_get") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_get() requires two arguments (map, key)");
        }

        // Get or declare aria_map_get_simple
        llvm::Function* func = module->getFunction("aria_map_get_simple");
        if (!func) {
            // void* aria_map_get_simple(AriaMap* map, const void* key)
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_map_get_simple", module);
        }

        llvm::Value* map_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);

        // If key is not already a pointer, allocate and store it
        if (!key_ptr->getType()->isPointerTy()) {
            llvm::Value* alloca = builder.CreateAlloca(key_ptr->getType(), nullptr, "key_temp");
            builder.CreateStore(key_ptr, alloca);
            key_ptr = alloca;
        }

        return builder.CreateCall(func, {map_ptr, key_ptr}, "map_value");
    }

    // map_has(map: ptr, key: ptr) -> bool
    if (callee_ident->name == "map_has") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_has() requires two arguments (map, key)");
        }

        // Get or declare aria_map_has
        llvm::Function* func = module->getFunction("aria_map_has");
        if (!func) {
            // bool aria_map_has(const AriaMap* map, const void* key)
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt1Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_map_has", module);
        }

        llvm::Value* map_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);

        // If key is not already a pointer, allocate and store it
        if (!key_ptr->getType()->isPointerTy()) {
            llvm::Value* alloca = builder.CreateAlloca(key_ptr->getType(), nullptr, "key_temp");
            builder.CreateStore(key_ptr, alloca);
            key_ptr = alloca;
        }

        return builder.CreateCall(func, {map_ptr, key_ptr}, "map_has_key");
    }

    // map_remove(map: ptr, key: ptr) -> void
    if (callee_ident->name == "map_remove") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_remove() requires two arguments (map, key)");
        }

        // Get or declare aria_map_remove (we'll use the full Result version since simple version doesn't exist)
        llvm::Function* func = module->getFunction("aria_map_remove");
        if (!func) {
            // AriaResultVoid aria_map_remove(AriaMap* map, const void* key)
            // For now, we'll create a void version wrapper
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getVoidTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_map_remove", module);
        }

        llvm::Value* map_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);

        // If key is not already a pointer, allocate and store it
        if (!key_ptr->getType()->isPointerTy()) {
            llvm::Value* alloca = builder.CreateAlloca(key_ptr->getType(), nullptr, "key_temp");
            builder.CreateStore(key_ptr, alloca);
            key_ptr = alloca;
        }

        builder.CreateCall(func, {map_ptr, key_ptr});
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);  // Return 0 on success
    }

    // ====================================================================
    // TERNARY LOGIC BUILTINS
    // ====================================================================
    
    // trit_and(a: trit, b: trit) -> trit
    if (callee_ident->name == "trit_and") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("trit_and() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_trit_and");
        if (!func) {
            // int8_t aria_trit_and(int8_t a, int8_t b)
            std::vector<llvm::Type*> params = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt8Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_trit_and", module);
        }
        
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        
        // No type conversion needed - trit is now i8, matching runtime expectation
        llvm::Value* result = builder.CreateCall(func, {a, b}, "trit_and_call");
        return result;
    }
    
    // trit_or(a: trit, b: trit) -> trit
    if (callee_ident->name == "trit_or") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("trit_or() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_trit_or");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt8Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_trit_or", module);
        }
        
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        
        // No type conversion needed - trit is now i8, matching runtime expectation
        llvm::Value* result = builder.CreateCall(func, {a, b}, "trit_or_call");
        return result;
    }
    
    // nit_and(a: nit, b: nit) -> nit
    if (callee_ident->name == "nit_and") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("nit_and() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_nit_and");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt8Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_nit_and", module);
        }
        
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        
        // No type conversion needed - nit is now i8, matching runtime expectation
        llvm::Value* result = builder.CreateCall(func, {a, b}, "nit_and_call");
        return result;
    }
    
    // nit_or(a: nit, b: nit) -> nit
    if (callee_ident->name == "nit_or") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("nit_or() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_nit_or");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt8Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_nit_or", module);
        }
        
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        
        // No type conversion needed - nit is now i8, matching runtime expectation
        llvm::Value* result = builder.CreateCall(func, {a, b}, "nit_or_call");
        return result;
    }
    
    // tbb8_from_int(value: int32) -> tbb8
    if (callee_ident->name == "tbb8_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_from_int() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_tbb8_from_int");
        if (!func) {
            // int8_t aria_tbb8_from_int(int32_t value)
            std::vector<llvm::Type*> params = {builder.getInt32Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt8Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_tbb8_from_int", module);
        }
        
        llvm::Value* value = codegenExpressionNode(expr->arguments[0].get(), this);
        // Ensure it's i32
        if (value->getType()->getIntegerBitWidth() != 32) {
            value = builder.CreateSExtOrTrunc(value, builder.getInt32Ty());
        }
        return builder.CreateCall(func, {value}, "tbb8_from_int");
    }
    
    // tbb8_to_int(value: tbb8) -> int32
    if (callee_ident->name == "tbb8_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_to_int() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_tbb8_to_int");
        if (!func) {
            // int32_t aria_tbb8_to_int(int8_t value)
            std::vector<llvm::Type*> params = {builder.getInt8Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt32Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_tbb8_to_int", module);
        }
        
        llvm::Value* value = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {value}, "tbb8_to_int");
    }
    
    // tbb8_is_err(value: tbb8) -> bool
    if (callee_ident->name == "tbb8_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_is_err() requires one argument");
        }
        
        llvm::Value* value = codegenExpressionNode(expr->arguments[0].get(), this);
        // Check if value == -128 (ERR sentinel)
        llvm::Value* err_sentinel = llvm::ConstantInt::get(builder.getInt8Ty(), -128);
        return builder.CreateICmpEQ(value, err_sentinel, "is_err");
    }
    
    // tbb16_from_int(value: int32) -> tbb16
    if (callee_ident->name == "tbb16_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_from_int() requires one argument");
        }
        
        llvm::Value* value = codegenExpressionNode(expr->arguments[0].get(), this);
        // Check range [-32767, 32767], return -32768 (ERR) if out of range
        llvm::Value* min_val = llvm::ConstantInt::get(builder.getInt32Ty(), -32767);
        llvm::Value* max_val = llvm::ConstantInt::get(builder.getInt32Ty(), 32767);
        llvm::Value* err_val = llvm::ConstantInt::get(builder.getInt16Ty(), -32768);
        
        llvm::Value* is_too_low = builder.CreateICmpSLT(value, min_val);
        llvm::Value* is_too_high = builder.CreateICmpSGT(value, max_val);
        llvm::Value* is_out_of_range = builder.CreateOr(is_too_low, is_too_high);
        
        llvm::Value* truncated = builder.CreateTrunc(value, builder.getInt16Ty());
        return builder.CreateSelect(is_out_of_range, err_val, truncated, "tbb16_from_int");
    }
    
    // tbb16_is_err(value: tbb16) -> bool
    if (callee_ident->name == "tbb16_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_is_err() requires one argument");
        }
        
        llvm::Value* value = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* err_sentinel = llvm::ConstantInt::get(builder.getInt16Ty(), -32768);
        return builder.CreateICmpEQ(value, err_sentinel, "is_err");
    }
    
    // tbb32_from_int(value: int32) -> tbb32
    if (callee_ident->name == "tbb32_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_from_int() requires one argument");
        }
        
        // For tbb32, we can't overflow from int32 input
        // tbb32 range is [-2147483647, 2147483647], sentinel is -2147483648 (INT32_MIN)
        // Since input is int32, only INT32_MIN is the error case
        llvm::Value* value = codegenExpressionNode(expr->arguments[0].get(), this);
        return value;  // Direct passthrough for int32 -> tbb32
    }
    
    // tbb32_to_int(value: tbb32) -> int32
    if (callee_ident->name == "tbb32_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_to_int() requires one argument");
        }
        
        llvm::Value* value = codegenExpressionNode(expr->arguments[0].get(), this);
        return value;  // Direct passthrough for tbb32 -> int32
    }

    // ====================================================================
    // FILE I/O BUILTINS (Phase 4.2)
    // ====================================================================
    
    // readFile(path: string) -> string
    // Calls aria_read_file_simple which returns AriaString directly
    if (callee_ident->name == "readFile") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("readFile() requires one argument (path)");
        }
        
        // Get or declare aria_read_file_simple
        llvm::Function* func = module->getFunction("aria_read_file_simple");
        if (!func) {
            // AriaString aria_read_file_simple(const char* path)
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(builder.getInt8Ty(), 0)  // const char*
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_read_file_simple", module);
        }
        
        // Get path argument (should be AriaString)
        llvm::Value* path_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* path_str = builder.CreateLoad(getAriaStringType(), path_ptr, "path_str");
        (void)path_str;
        
        // Extract char* from AriaString
        llvm::Value* path_data_ptr = builder.CreateStructGEP(getAriaStringType(), 
            path_ptr, 0, "path_data_ptr");
        llvm::Value* path_cstr = builder.CreateLoad(
            llvm::PointerType::get(builder.getInt8Ty(), 0), path_data_ptr, "path_cstr");
        
        return builder.CreateCall(func, {path_cstr}, "read_result");
    }
    
    // writeFile(path: string, content: string) -> int64
    // Returns 0 on success, -1 on error
    if (callee_ident->name == "writeFile") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("writeFile() requires two arguments (path, content)");
        }
        
        // Get or declare aria_write_file_simple
        llvm::Function* func = module->getFunction("aria_write_file_simple");
        if (!func) {
            // int64_t aria_write_file_simple(const char* path, const char* content)
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(builder.getInt8Ty(), 0),  // const char* path
                llvm::PointerType::get(builder.getInt8Ty(), 0)   // const char* content
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_write_file_simple", module);
        }
        
        // Get path argument
        llvm::Value* path_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* path_data_ptr = builder.CreateStructGEP(getAriaStringType(), 
            path_ptr, 0, "path_data_ptr");
        llvm::Value* path_cstr = builder.CreateLoad(
            llvm::PointerType::get(builder.getInt8Ty(), 0), path_data_ptr, "path_cstr");
        
        // Get content argument
        llvm::Value* content_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* content_data_ptr = builder.CreateStructGEP(getAriaStringType(), 
            content_ptr, 0, "content_data_ptr");
        llvm::Value* content_cstr = builder.CreateLoad(
            llvm::PointerType::get(builder.getInt8Ty(), 0), content_data_ptr, "content_cstr");
        
        return builder.CreateCall(func, {path_cstr, content_cstr}, "write_result");
    }
    
    // allocate(size: int32) -> buffer@ (void*)
    // Allocates wild memory heap allocation
    if (callee_ident->name == "allocate") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("allocate() requires one argument (size)");
        }
        
        // Get or declare aria_alloc
        llvm::Function* func = module->getFunction("aria_alloc");
        if (!func) {
            // void* aria_alloc(size_t size)
            std::vector<llvm::Type*> params = {
                builder.getInt64Ty()  // size_t size (using i64 for size_t)
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(builder.getInt8Ty(), 0),  // void* return
                params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_alloc", module);
        }
        
        // Codegen size argument (should be int32/int64)
        llvm::Value* size = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Extend to i64 if needed (from int32)
        if (size->getType()->isIntegerTy(32)) {
            size = builder.CreateSExt(size, builder.getInt64Ty(), "size_i64");
        }
        
        return builder.CreateCall(func, {size}, "alloc_ptr");
    }
    
    // fileExists(path: string) -> bool
    if (callee_ident->name == "fileExists") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("fileExists() requires one argument (path)");
        }
        
        // Get or declare aria_file_exists
        llvm::Function* func = module->getFunction("aria_file_exists");
        if (!func) {
            // bool aria_file_exists(const char* path)
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(builder.getInt8Ty(), 0)
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt1Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_file_exists", module);
        }
        
        // Get path argument
        llvm::Value* path_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* path_data_ptr = builder.CreateStructGEP(getAriaStringType(), 
            path_ptr, 0, "path_data_ptr");
        llvm::Value* path_cstr = builder.CreateLoad(
            llvm::PointerType::get(builder.getInt8Ty(), 0), path_data_ptr, "path_cstr");
        
        return builder.CreateCall(func, {path_cstr}, "exists_result");
    }
    
    // fileSize(path: string) -> int64
    if (callee_ident->name == "fileSize") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("fileSize() requires one argument (path)");
        }
        
        // Get or declare aria_file_size
        llvm::Function* func = module->getFunction("aria_file_size");
        if (!func) {
            // int64_t aria_file_size(const char* path)
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(builder.getInt8Ty(), 0)
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_file_size", module);
        }
        
        // Get path argument
        llvm::Value* path_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* path_data_ptr = builder.CreateStructGEP(getAriaStringType(), 
            path_ptr, 0, "path_data_ptr");
        llvm::Value* path_cstr = builder.CreateLoad(
            llvm::PointerType::get(builder.getInt8Ty(), 0), path_data_ptr, "path_cstr");
        
        return builder.CreateCall(func, {path_cstr}, "size_result");
    }
    
    // deleteFile(path: string) -> int64
    if (callee_ident->name == "deleteFile") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("deleteFile() requires one argument (path)");
        }
        
        // Get or declare aria_delete_file_simple
        llvm::Function* func = module->getFunction("aria_delete_file_simple");
        if (!func) {
            // int64_t aria_delete_file_simple(const char* path)
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(builder.getInt8Ty(), 0)
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_delete_file_simple", module);
        }
        
        // Get path argument
        llvm::Value* path_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* path_data_ptr = builder.CreateStructGEP(getAriaStringType(), 
            path_ptr, 0, "path_data_ptr");
        llvm::Value* path_cstr = builder.CreateLoad(
            llvm::PointerType::get(builder.getInt8Ty(), 0), path_data_ptr, "path_cstr");
        
        return builder.CreateCall(func, {path_cstr}, "delete_result");
    }
    
    // ====================================================================
    // COMPILER INTRINSICS - Bit Manipulation & Performance Hints
    // ====================================================================
    
    // @clz32/64 - Count leading zeros
    if (callee_ident->name == "@clz32" || callee_ident->name == "clz32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("clz32() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* ctlz = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::ctlz, {builder.getInt32Ty()});
        llvm::Value* is_zero_undef = builder.getInt1(false);  // Return bit-width if zero
        return builder.CreateCall(ctlz, {arg, is_zero_undef}, "clz");
    }
    
    if (callee_ident->name == "@clz64" || callee_ident->name == "clz64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("clz64() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* ctlz = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::ctlz, {builder.getInt64Ty()});
        llvm::Value* is_zero_undef = builder.getInt1(false);
        llvm::Value* result64 = builder.CreateCall(ctlz, {arg, is_zero_undef}, "clz");
        return builder.CreateTrunc(result64, builder.getInt32Ty(), "clz32");
    }
    
    // @ctz32/64 - Count trailing zeros
    if (callee_ident->name == "@ctz32" || callee_ident->name == "ctz32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("ctz32() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* cttz = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::cttz, {builder.getInt32Ty()});
        llvm::Value* is_zero_undef = builder.getInt1(false);
        return builder.CreateCall(cttz, {arg, is_zero_undef}, "ctz");
    }
    
    if (callee_ident->name == "@ctz64" || callee_ident->name == "ctz64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("ctz64() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* cttz = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::cttz, {builder.getInt64Ty()});
        llvm::Value* is_zero_undef = builder.getInt1(false);
        llvm::Value* result64 = builder.CreateCall(cttz, {arg, is_zero_undef}, "ctz");
        return builder.CreateTrunc(result64, builder.getInt32Ty(), "ctz32");
    }
    
    // @popcount32/64 - Count set bits
    if (callee_ident->name == "@popcount32" || callee_ident->name == "popcount32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("popcount32() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* ctpop = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::ctpop, {builder.getInt32Ty()});
        return builder.CreateCall(ctpop, {arg}, "popcount");
    }
    
    if (callee_ident->name == "@popcount64" || callee_ident->name == "popcount64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("popcount64() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* ctpop = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::ctpop, {builder.getInt64Ty()});
        llvm::Value* result64 = builder.CreateCall(ctpop, {arg}, "popcount");
        return builder.CreateTrunc(result64, builder.getInt32Ty(), "popcount32");
    }
    
    // @bswap16/32/64 - Byte swap
    if (callee_ident->name == "@bswap16" || callee_ident->name == "bswap16") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("bswap16() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* bswap = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::bswap, {builder.getInt16Ty()});
        return builder.CreateCall(bswap, {arg}, "bswap");
    }
    
    if (callee_ident->name == "@bswap32" || callee_ident->name == "bswap32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("bswap32() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* bswap = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::bswap, {builder.getInt32Ty()});
        return builder.CreateCall(bswap, {arg}, "bswap");
    }
    
    if (callee_ident->name == "@bswap64" || callee_ident->name == "bswap64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("bswap64() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* bswap = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::bswap, {builder.getInt64Ty()});
        return builder.CreateCall(bswap, {arg}, "bswap");
    }
    
    // @likely/unlikely - Branch prediction hints
    if (callee_ident->name == "@likely" || callee_ident->name == "likely") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("likely() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* expect = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::expect, {builder.getInt1Ty()});
        llvm::Value* true_val = builder.getInt1(true);
        return builder.CreateCall(expect, {arg, true_val}, "likely");
    }
    
    if (callee_ident->name == "@unlikely" || callee_ident->name == "unlikely") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("unlikely() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* expect = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::expect, {builder.getInt1Ty()});
        llvm::Value* false_val = builder.getInt1(false);
        return builder.CreateCall(expect, {arg, false_val}, "unlikely");
    }
    
    // @prefetch - Cache prefetch hint
    if (callee_ident->name == "@prefetch" || callee_ident->name == "prefetch") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("prefetch() requires one argument");
        }
        llvm::Value* ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* prefetch = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::prefetch, {ptr->getType()});
        // rw=0 (read), locality=3 (high), cache type=1 (data)
        llvm::Value* rw = builder.getInt32(0);
        llvm::Value* locality = builder.getInt32(3);
        llvm::Value* cache_type = builder.getInt32(1);
        builder.CreateCall(prefetch, {ptr, rw, locality, cache_type});
        // Return void - but need a placeholder value
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);
    }
    
    // @breakpoint - Debugger breakpoint
    if (callee_ident->name == "@breakpoint" || callee_ident->name == "breakpoint") {
        llvm::Function* debugtrap = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::debugtrap);
        builder.CreateCall(debugtrap);
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);
    }
    
    // @trap - Intentional trap/abort
    if (callee_ident->name == "@trap" || callee_ident->name == "trap") {
        llvm::Function* trap = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::trap);
        builder.CreateCall(trap);
        // Insert unreachable after trap
        builder.CreateUnreachable();
        // Return dummy value (dead code)
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);
    }
    
    // @unreachable - Mark code as unreachable
    if (callee_ident->name == "@unreachable" || callee_ident->name == "unreachable") {
        builder.CreateUnreachable();
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);
    }
    
    // @sizeof(type_or_expr) - Returns size of type in bytes as int64
    if (callee_ident->name == "@sizeof" || callee_ident->name == "sizeof") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("@sizeof() requires exactly one argument");
        }
        
        llvm::Type* target_type = nullptr;
        
        // Check if the argument is an identifier (type name or variable)
        if (expr->arguments[0]->type == ASTNode::NodeType::IDENTIFIER) {
            auto* id = static_cast<IdentifierExpr*>(expr->arguments[0].get());
            
            // First try as a type name
            target_type = getLLVMTypeFromString(id->name);
            
            // If not a type name, try as a variable — get its stored type
            if (!target_type) {
                auto it = named_values.find(id->name);
                if (it != named_values.end()) {
                    llvm::Value* val = it->second;
                    // If it's an alloca, get the allocated type
                    if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(val)) {
                        target_type = alloca->getAllocatedType();
                    } else {
                        target_type = val->getType();
                    }
                }
            }
        }
        
        if (!target_type) {
            // Last resort: codegen the expression and use its type
            llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
            if (val) {
                target_type = val->getType();
            }
        }
        
        if (!target_type) {
            throw std::runtime_error("@sizeof: cannot determine type of argument");
        }
        
        // Use DataLayout to compute size in bytes
        const llvm::DataLayout& dl = module->getDataLayout();
        uint64_t size = dl.getTypeAllocSize(target_type);
        return llvm::ConstantInt::get(builder.getInt64Ty(), size);
    }
    
    // ====================================================================
    // MATH LIBRARY BUILTINS (Phase 4.4)
    // ====================================================================
    
    // abs() - absolute value (works with int and float types)
    if (callee_ident->name == "abs") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("abs() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        
        if (arg->getType()->isIntegerTy()) {
            // Integer absolute value: select(x < 0, -x, x)
            llvm::Value* zero = llvm::ConstantInt::get(arg->getType(), 0);
            llvm::Value* is_negative = builder.CreateICmpSLT(arg, zero, "is_neg");
            llvm::Value* negated = builder.CreateNeg(arg, "negated");
            return builder.CreateSelect(is_negative, negated, arg, "abs");
        } else if (arg->getType()->isDoubleTy()) {
            // Float absolute value: llvm.fabs intrinsic
            llvm::Function* fabs_func = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::fabs, {arg->getType()});
            return builder.CreateCall(fabs_func, {arg}, "abs");
        } else if (arg->getType()->isFloatTy()) {
            llvm::Function* fabs_func = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::fabs, {arg->getType()});
            return builder.CreateCall(fabs_func, {arg}, "abs");
        }
        throw std::runtime_error("abs() requires numeric type");
    }
    
    // min() and max()
    if (callee_ident->name == "min" || callee_ident->name == "max") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error(callee_ident->name + "() requires two arguments");
        }
        llvm::Value* arg1 = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* arg2 = codegenExpressionNode(expr->arguments[1].get(), this);
        
        bool is_min = (callee_ident->name == "min");
        
        if (arg1->getType()->isIntegerTy()) {
            llvm::Value* cmp = is_min ? 
                builder.CreateICmpSLT(arg1, arg2, "cmp") :
                builder.CreateICmpSGT(arg1, arg2, "cmp");
            return builder.CreateSelect(cmp, arg1, arg2, callee_ident->name);
        } else if (arg1->getType()->isFloatingPointTy()) {
            // Use LLVM intrinsics for float min/max
            llvm::Intrinsic::ID intrinsic_id = is_min ? 
                llvm::Intrinsic::minnum : llvm::Intrinsic::maxnum;
            llvm::Function* minmax_func = llvm::Intrinsic::getOrInsertDeclaration(
                module, intrinsic_id, {arg1->getType()});
            return builder.CreateCall(minmax_func, {arg1, arg2}, callee_ident->name);
        }
        throw std::runtime_error(callee_ident->name + "() requires numeric types");
    }
    
    // Rounding functions: floor, ceil, round, trunc
    if (callee_ident->name == "floor" || callee_ident->name == "ceil" ||
        callee_ident->name == "round" || callee_ident->name == "trunc") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error(callee_ident->name + "() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Convert to double if needed
        if (!arg->getType()->isDoubleTy()) {
            if (arg->getType()->isFloatTy()) {
                arg = builder.CreateFPExt(arg, builder.getDoubleTy());
            } else if (arg->getType()->isIntegerTy()) {
                arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
            }
        }
        
        llvm::Intrinsic::ID intrinsic_id;
        if (callee_ident->name == "floor") intrinsic_id = llvm::Intrinsic::floor;
        else if (callee_ident->name == "ceil") intrinsic_id = llvm::Intrinsic::ceil;
        else if (callee_ident->name == "round") intrinsic_id = llvm::Intrinsic::round;
        else intrinsic_id = llvm::Intrinsic::trunc;
        
        llvm::Function* func = llvm::Intrinsic::getOrInsertDeclaration(
            module, intrinsic_id, {builder.getDoubleTy()});
        return builder.CreateCall(func, {arg}, callee_ident->name);
    }
    
    // sqrt()
    if (callee_ident->name == "sqrt") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("sqrt() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg->getType()->isDoubleTy()) {
            if (arg->getType()->isFloatTy()) {
                arg = builder.CreateFPExt(arg, builder.getDoubleTy());
            } else if (arg->getType()->isIntegerTy()) {
                arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
            }
        }
        llvm::Function* sqrt_func = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::sqrt, {builder.getDoubleTy()});
        return builder.CreateCall(sqrt_func, {arg}, "sqrt");
    }

    // cbrt() - cube root via libm
    if (callee_ident->name == "cbrt") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("cbrt() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg->getType()->isDoubleTy()) {
            if (arg->getType()->isFloatTy()) {
                arg = builder.CreateFPExt(arg, builder.getDoubleTy());
            } else if (arg->getType()->isIntegerTy()) {
                arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
            }
        }

        // Declare libm cbrt function
        llvm::Function* cbrt_func = module->getFunction("cbrt");
        if (!cbrt_func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getDoubleTy(), {builder.getDoubleTy()}, false);
            cbrt_func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "cbrt", module);
        }
        return builder.CreateCall(cbrt_func, {arg}, "cbrt");
    }

    // pow()
    if (callee_ident->name == "pow") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("pow() requires two arguments");
        }
        llvm::Value* base = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* exp = codegenExpressionNode(expr->arguments[1].get(), this);
        
        if (!base->getType()->isDoubleTy()) {
            if (base->getType()->isFloatTy()) {
                base = builder.CreateFPExt(base, builder.getDoubleTy());
            } else if (base->getType()->isIntegerTy()) {
                base = builder.CreateSIToFP(base, builder.getDoubleTy());
            }
        }
        if (!exp->getType()->isDoubleTy()) {
            if (exp->getType()->isFloatTy()) {
                exp = builder.CreateFPExt(exp, builder.getDoubleTy());
            } else if (exp->getType()->isIntegerTy()) {
                exp = builder.CreateSIToFP(exp, builder.getDoubleTy());
            }
        }
        
        llvm::Function* pow_func = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::pow, {builder.getDoubleTy()});
        return builder.CreateCall(pow_func, {base, exp}, "pow");
    }
    
    // exp()
    if (callee_ident->name == "exp") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("exp() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg->getType()->isDoubleTy()) {
            if (arg->getType()->isFloatTy()) {
                arg = builder.CreateFPExt(arg, builder.getDoubleTy());
            } else if (arg->getType()->isIntegerTy()) {
                arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
            }
        }
        llvm::Function* exp_func = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::exp, {builder.getDoubleTy()});
        return builder.CreateCall(exp_func, {arg}, "exp");
    }
    
    // log(), ln(), log10(), log2() - ln is alias for natural log
    if (callee_ident->name == "log" || callee_ident->name == "ln" ||
        callee_ident->name == "log10" || callee_ident->name == "log2") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error(callee_ident->name + "() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg->getType()->isDoubleTy()) {
            if (arg->getType()->isFloatTy()) {
                arg = builder.CreateFPExt(arg, builder.getDoubleTy());
            } else if (arg->getType()->isIntegerTy()) {
                arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
            }
        }
        
        llvm::Intrinsic::ID intrinsic_id;
        if (callee_ident->name == "log" || callee_ident->name == "ln")
            intrinsic_id = llvm::Intrinsic::log;  // ln is alias for natural log
        else if (callee_ident->name == "log10") intrinsic_id = llvm::Intrinsic::log10;
        else intrinsic_id = llvm::Intrinsic::log2;
        
        llvm::Function* log_func = llvm::Intrinsic::getOrInsertDeclaration(
            module, intrinsic_id, {builder.getDoubleTy()});
        return builder.CreateCall(log_func, {arg}, callee_ident->name);
    }
    
    // Trigonometric functions: sin, cos
    if (callee_ident->name == "sin" || callee_ident->name == "cos") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error(callee_ident->name + "() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg->getType()->isDoubleTy()) {
            if (arg->getType()->isFloatTy()) {
                arg = builder.CreateFPExt(arg, builder.getDoubleTy());
            } else if (arg->getType()->isIntegerTy()) {
                arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
            }
        }
        
        llvm::Intrinsic::ID intrinsic_id = (callee_ident->name == "sin") ? 
            llvm::Intrinsic::sin : llvm::Intrinsic::cos;
        llvm::Function* trig_func = llvm::Intrinsic::getOrInsertDeclaration(
            module, intrinsic_id, {builder.getDoubleTy()});
        return builder.CreateCall(trig_func, {arg}, callee_ident->name);
    }
    
    // Trigonometric functions requiring libm: tan, asin, acos, atan, atan2
    if (callee_ident->name == "tan" || callee_ident->name == "asin" || 
        callee_ident->name == "acos" || callee_ident->name == "atan") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error(callee_ident->name + "() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg->getType()->isDoubleTy()) {
            if (arg->getType()->isFloatTy()) {
                arg = builder.CreateFPExt(arg, builder.getDoubleTy());
            } else if (arg->getType()->isIntegerTy()) {
                arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
            }
        }
        
        // Declare libm function
        llvm::Function* libm_func = module->getFunction(callee_ident->name);
        if (!libm_func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getDoubleTy(), {builder.getDoubleTy()}, false);
            libm_func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                callee_ident->name, module);
        }
        return builder.CreateCall(libm_func, {arg}, callee_ident->name);
    }
    
    if (callee_ident->name == "atan2") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("atan2() requires two arguments");
        }
        llvm::Value* y = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* x = codegenExpressionNode(expr->arguments[1].get(), this);
        
        if (!y->getType()->isDoubleTy()) {
            if (y->getType()->isFloatTy()) {
                y = builder.CreateFPExt(y, builder.getDoubleTy());
            } else if (y->getType()->isIntegerTy()) {
                y = builder.CreateSIToFP(y, builder.getDoubleTy());
            }
        }
        if (!x->getType()->isDoubleTy()) {
            if (x->getType()->isFloatTy()) {
                x = builder.CreateFPExt(x, builder.getDoubleTy());
            } else if (x->getType()->isIntegerTy()) {
                x = builder.CreateSIToFP(x, builder.getDoubleTy());
            }
        }
        
        llvm::Function* atan2_func = module->getFunction("atan2");
        if (!atan2_func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getDoubleTy(), {builder.getDoubleTy(), builder.getDoubleTy()}, false);
            atan2_func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "atan2", module);
        }
        return builder.CreateCall(atan2_func, {y, x}, "atan2");
    }
    
    // Mathematical constants: PI, E, TAU
    if (callee_ident->name == "PI") {
        if (expr->arguments.size() != 0) {
            throw std::runtime_error("PI() takes no arguments");
        }
        return llvm::ConstantFP::get(builder.getDoubleTy(), 3.14159265358979323846);
    }
    
    if (callee_ident->name == "E") {
        if (expr->arguments.size() != 0) {
            throw std::runtime_error("E() takes no arguments");
        }
        return llvm::ConstantFP::get(builder.getDoubleTy(), 2.71828182845904523536);
    }
    
    if (callee_ident->name == "TAU") {
        if (expr->arguments.size() != 0) {
            throw std::runtime_error("TAU() takes no arguments");
        }
        return llvm::ConstantFP::get(builder.getDoubleTy(), 6.28318530717958647692);
    }
    
    // ====================================================================
    // END MATH BUILTINS
    // ====================================================================
    
    // Check if this is a direct function call or a closure call
    // For generic functions, use the specialized mangled name if available
    std::string func_name = !expr->specializedMangledName.empty() 
                            ? expr->specializedMangledName 
                            : callee_ident->name;
    
    // Try to find a direct function first
    llvm::Function* direct_func = module->getFunction(func_name);
    
    // Check if it's a closure (func variable in named_values)
    auto it = named_values.find(callee_ident->name);
    bool is_closure_call = (direct_func == nullptr && it != named_values.end());
    
    if (is_closure_call) {
        // ====================================================================
        // CLOSURE CALLING CONVENTION (Fat Pointer Call)
        // ====================================================================
        // Fat pointer struct: { i8* method_ptr, i8* env_ptr }
        // Calling convention: call method_ptr(env_ptr, explicit_args...)
        
        llvm::Value* fat_ptr_alloca = it->second;
        
        // Load the fat pointer struct from memory
        // Define the fat pointer struct type
        std::vector<llvm::Type*> fat_ptr_fields = {
            llvm::PointerType::get(context, 0),  // method_ptr
            llvm::PointerType::get(context, 0)   // env_ptr
        };
        llvm::StructType* fat_ptr_type = llvm::StructType::get(context, fat_ptr_fields);
        
        llvm::Value* fat_ptr_value = builder.CreateLoad(fat_ptr_type, fat_ptr_alloca, "fat_ptr");
        
        // Extract method_ptr (field 0)
        llvm::Value* method_ptr = builder.CreateExtractValue(fat_ptr_value, 0, "method_ptr");
        
        // Extract env_ptr (field 1)
        llvm::Value* env_ptr = builder.CreateExtractValue(fat_ptr_value, 1, "env_ptr");
        
        // Evaluate explicit arguments
        std::vector<llvm::Value*> args;
        
        // Hidden first argument: env_ptr
        args.push_back(env_ptr);
        
        // Explicit arguments
        for (size_t i = 0; i < expr->arguments.size(); i++) {
            llvm::Value* arg_value = codegenExpressionNode(expr->arguments[i].get(), this);
            if (!arg_value) {
                throw std::runtime_error("Failed to generate code for closure argument " + std::to_string(i));
            }
            args.push_back(arg_value);
        }
        
        // Build function type for the call
        // Return type: Need type inference - for now try to infer from variable or default to i64
        // Parameters: env_ptr (i8*) + explicit arg types
        std::vector<llvm::Type*> param_types;
        param_types.push_back(llvm::PointerType::get(context, 0));  // env_ptr
        for (size_t i = 1; i < args.size(); i++) {
            param_types.push_back(args[i]->getType());
        }
        
        // Try to determine return type from var_aria_types
        // func_ptr variables are stored as "func_ptr:retTypeName"
        llvm::Type* return_type = llvm::Type::getInt64Ty(context); // default fallback
        {
            auto type_it = var_aria_types.find(callee_ident->name);
            if (type_it != var_aria_types.end()) {
                const std::string& aria_type = type_it->second;
                if (aria_type.size() > 9 && aria_type.substr(0, 9) == "func_ptr:") {
                    std::string ret_str = aria_type.substr(9);
                    if (ret_str == "void") {
                        return_type = llvm::Type::getVoidTy(context);
                    } else {
                        llvm::Type* inner_type = getLLVMTypeFromString(ret_str);
                        if (inner_type) {
                            // Wrap in Result<T>: { T, i8*, i8 } matching module-level functions
                            return_type = llvm::StructType::get(context, {
                                inner_type,
                                llvm::PointerType::get(context, 0),
                                llvm::Type::getInt8Ty(context)
                            });
                        }
                    }
                }
            }
        }
        
        llvm::FunctionType* closure_func_type = llvm::FunctionType::get(
            return_type,
            param_types,
            false  // not vararg
        );
        
        // In LLVM 16+ (opaque pointer mode), all pointers are plain `ptr`.
        // No bitcast is needed — method_ptr is already `ptr`, and CreateCall
        // takes the FunctionType + a ptr callee directly.
        
        // Create indirect call through function pointer
        return builder.CreateCall(closure_func_type, method_ptr, args, "closure_call");
        
    } else if (direct_func) {
        // ====================================================================
        // DIRECT FUNCTION CALL (Standard calling convention)
        // ====================================================================
        
        // Verify argument count matches
        if (direct_func->arg_size() != expr->arguments.size()) {
            throw std::runtime_error("Incorrect number of arguments passed to function " + 
                                    callee_ident->name + ": expected " + 
                                    std::to_string(direct_func->arg_size()) + 
                                    ", got " + std::to_string(expr->arguments.size()));
        }
        
        // Evaluate all arguments recursively
        std::vector<llvm::Value*> args;
        for (size_t i = 0; i < expr->arguments.size(); i++) {
            bool mutableBorrowParam =
                var_aria_types.count("__func_borrow_param:" + func_name + ":" + std::to_string(i)) &&
                var_aria_types["__func_borrow_param:" + func_name + ":" + std::to_string(i)] == "mut";

            llvm::Value* arg_value = nullptr;
            if (mutableBorrowParam) {
                // v0.18.0: `$$m T:param` expects an address. The parameter
                // declaration carries the borrow contract; the call site passes
                // the addressable variable directly.
                ASTNode* argNode = expr->arguments[i].get();
                if (argNode && argNode->type == ASTNode::NodeType::IDENTIFIER) {
                    auto* ident = static_cast<IdentifierExpr*>(argNode);
                    auto storageIt = named_values.find(ident->name);
                    if (storageIt == named_values.end()) {
                        throw std::runtime_error("Undefined variable for mutable borrow: " + ident->name);
                    }

                    arg_value = storageIt->second;
                    if (!arg_value->getType()->isPointerTy()) {
                        llvm::AllocaInst* tmp = builder.CreateAlloca(
                            arg_value->getType(), nullptr, ident->name + ".borrow.addr");
                        builder.CreateStore(arg_value, tmp);
                        named_values[ident->name] = tmp;
                        arg_value = tmp;
                    }
                }

                if (!arg_value) {
                    throw std::runtime_error("Mutable borrow parameter requires an addressable identifier argument");
                }
            } else {
                arg_value = codegenExpressionNode(expr->arguments[i].get(), this);
            }
            if (!arg_value) {
                throw std::runtime_error("Failed to generate code for argument " + std::to_string(i));
            }
            
            // PIPELINE OPERATOR FIX: Auto-unwrap Result<T> → T for pipeline calls
            // When using |> or <|, if a function returns Result<T>, automatically 
            // extract the .val field when passing to the next function expecting T
            if (expr->isPipelineCall && arg_value->getType()->isStructTy()) {
                llvm::StructType* arg_struct = llvm::cast<llvm::StructType>(arg_value->getType());
                
                // Result types are structs with 3 fields: {T val, error* err, i8 is_error}
                // Check if this looks like a Result type by field count and types
                if (arg_struct->getNumElements() == 3 &&
                    arg_struct->getElementType(1)->isPointerTy() &&
                    (arg_struct->getElementType(2)->isIntegerTy(8) || arg_struct->getElementType(2)->isIntegerTy(1))) {
                    
                    // Extract the value field (field 0) from Result
                    arg_value = builder.CreateExtractValue(arg_value, 0, "pipeline_unwrap");
                }
            }
            
            // ARIA-026 FIX: LBIM ABI Scalarization for extern functions
            // CRITICAL FIX 1: AriaString data pointer extraction for FFI
            // Check if this is an extern function and the argument is an LBIM struct or AriaString
            bool is_extern = direct_func->hasExternalLinkage() && direct_func->isDeclaration();
            llvm::Type* arg_type = arg_value->getType();
            
            if (is_extern && arg_type->isStructTy()) {
                llvm::StructType* struct_type = llvm::cast<llvm::StructType>(arg_type);
                std::string struct_name = struct_type->hasName() ? struct_type->getName().str() : "";
                
                // CRITICAL FIX 1: Extract data pointer from AriaString when passing to C
                // AriaString is { i8* data, i64 length } but C functions expect char*
                // Without this, C receives address of struct instead of string data
                if (struct_name == "struct.AriaString") {
                    // Extract just the data pointer (field 0)
                    llvm::Value* data_ptr = builder.CreateExtractValue(arg_value, 0, "str_data");
                    arg_value = data_ptr;
                }
                // Check if this is an LBIM type (int128, uint128, int256, etc.)
                else if (struct_name.find("struct.int128") == 0 || struct_name.find("struct.uint128") == 0) {
                    // Scalarize int128: Extract 2 limbs and construct native i128
                    llvm::Value* limb0 = builder.CreateExtractValue(arg_value, {0, 0}, "limb0");
                    llvm::Value* limb1 = builder.CreateExtractValue(arg_value, {0, 1}, "limb1");
                    
                    // Construct i128: (limb1 << 64) | limb0
                    llvm::Type* i128Type = llvm::Type::getInt128Ty(context);
                    llvm::Value* limb0_ext = builder.CreateZExt(limb0, i128Type, "limb0_ext");
                    llvm::Value* limb1_ext = builder.CreateZExt(limb1, i128Type, "limb1_ext");
                    llvm::Value* limb1_shifted = builder.CreateShl(limb1_ext, 64, "limb1_shift");
                    llvm::Value* scalar_i128 = builder.CreateOr(limb0_ext, limb1_shifted, "scalar_i128");
                    
                    arg_value = scalar_i128;
                }
                // Check for larger LBIM types: pass by pointer for FFI (no native LLVM type)
                else if (struct_name.find("struct.int") == 0 || struct_name.find("struct.uint") == 0) {
                    // int256, int512, int1024, int2048, int4096 — alloca + store, pass pointer
                    llvm::AllocaInst* tmp = builder.CreateAlloca(arg_value->getType(), nullptr, "lbim_ffi_tmp");
                    builder.CreateStore(arg_value, tmp);
                    arg_value = tmp;
                }
            }
            
            // CRITICAL FIX 2: AriaString* pointer → char* for FFI (opaque pointer mode)
            // In LLVM opaque pointer mode, AriaString* has type 'ptr' — can't detect by type alone.
            // Detect via two sources:
            //   a) String LITERALS: arg_value is a GlobalVariable pointing to struct.AriaString
            //   b) String VARIABLES: identifier is in var_aria_types with type "string"
            if (is_extern && arg_type->isPointerTy()) {
                bool needs_data_extraction = false;
                
                // Case (a): String literal → GlobalVariable of type struct.AriaString
                if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(arg_value)) {
                    llvm::Type* val_type = gv->getValueType();
                    if (val_type->isStructTy()) {
                        llvm::StructType* st = llvm::cast<llvm::StructType>(val_type);
                        if (st->hasName() && st->getName() == "struct.AriaString") {
                            needs_data_extraction = true;
                        }
                    }
                }
                
                // Case (b): String variable → identifier with var_aria_types["name"] == "string"
                if (!needs_data_extraction && i < expr->arguments.size()) {
                    ASTNode* arg_node = expr->arguments[i].get();
                    if (arg_node->type == aria::ASTNode::NodeType::IDENTIFIER) {
                        IdentifierExpr* ident = static_cast<IdentifierExpr*>(arg_node);
                        auto type_it = var_aria_types.find(ident->name);
                        if (type_it != var_aria_types.end() && type_it->second == "string") {
                            needs_data_extraction = true;
                        }
                    }
                }
                
                if (needs_data_extraction) {
                    // Load AriaString struct from the AriaString* pointer, then extract .data
                    llvm::StructType* aria_string_type = getAriaStringType();
                    llvm::Value* str_struct = builder.CreateLoad(aria_string_type, arg_value, "str_struct_ffi");
                    arg_value = builder.CreateExtractValue(str_struct, 0, "str_data_ffi");
                }
            }

            // Array-to-pointer decay: flt64[N] → flt64@ (like C implicit array decay)
            // If the argument is an array type [N x T] but the parameter expects a pointer,
            // store the array in a temp alloca and pass a pointer to its first element.
            if (i < direct_func->getFunctionType()->getNumParams()) {
                llvm::Type* param_type = direct_func->getFunctionType()->getParamType(i);
                if (arg_value->getType()->isArrayTy() && param_type->isPointerTy()) {
                    llvm::Type* arr_type = arg_value->getType();
                    llvm::AllocaInst* arr_tmp = builder.CreateAlloca(arr_type, nullptr, "arr_decay_tmp");
                    builder.CreateStore(arg_value, arr_tmp);
                    arg_value = builder.CreateInBoundsGEP(
                        arr_type, arr_tmp,
                        {builder.getInt64(0), builder.getInt64(0)},
                        "arr_decay_ptr"
                    );
                }
                // Auto-unwrap Result<T> → T when passing a function-call result directly
                // as an argument to a function expecting the raw type.
                // Aria result structs are { T, ptr, i8 } (3-element, ptr at index 1, i8 at index 2).
                // v0.4.3: Check is_error first — if ERR, propagate to caller via early return.
                if (arg_value->getType() != param_type &&
                    arg_value->getType()->isStructTy()) {
                    llvm::StructType* arg_struct = llvm::cast<llvm::StructType>(arg_value->getType());
                    if (arg_struct->getNumElements() == 3 &&
                        arg_struct->getElementType(1)->isPointerTy() &&
                        arg_struct->getElementType(2)->isIntegerTy(8) &&
                        arg_struct->getElementType(0) == param_type) {
                        // Extract is_error flag (field 2)
                        llvm::Value* isError = builder.CreateExtractValue(arg_value, 2, "arg_is_error");
                        // Compare: is_error != 0
                        llvm::Value* isErr = builder.CreateICmpNE(isError,
                            llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 0), "arg_err_check");

                        llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();
                        llvm::BasicBlock* errBlock = llvm::BasicBlock::Create(context, "arg_err_prop", currentFunc);
                        llvm::BasicBlock* okBlock = llvm::BasicBlock::Create(context, "arg_ok", currentFunc);

                        builder.CreateCondBr(isErr, errBlock, okBlock);

                        // Error block: propagate error to caller (or jump to defaults fallback)
                        builder.SetInsertPoint(errBlock);
                        if (!defaults_fallback_stack_.empty()) {
                            // Inside a defaults/?| scope — jump to the fallback block
                            builder.CreateBr(defaults_fallback_stack_.back());
                        } else {
                            // No defaults scope — propagate ERR to caller via early return
                            llvm::Type* retType = currentFunc->getReturnType();
                            if (retType->isStructTy()) {
                                llvm::StructType* retStruct = llvm::cast<llvm::StructType>(retType);
                                llvm::Value* errResult = llvm::UndefValue::get(retStruct);
                                // Field 0: zero value
                                errResult = builder.CreateInsertValue(errResult,
                                    llvm::Constant::getNullValue(retStruct->getElementType(0)), 0, "prop.val");
                                // Field 1: forward the error pointer from the arg
                                llvm::Value* errPtr = builder.CreateExtractValue(arg_value, 1, "prop.err_ptr");
                                errResult = builder.CreateInsertValue(errResult, errPtr, 1, "prop.err");
                                // Field 2: is_error = 1
                                errResult = builder.CreateInsertValue(errResult,
                                    llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 1), 2, "prop.is_error");
                                builder.CreateRet(errResult);
                            } else {
                                // Non-Result return type — return zero/null
                                builder.CreateRet(llvm::Constant::getNullValue(retType));
                            }
                        }

                        // OK block: extract the value and continue
                        builder.SetInsertPoint(okBlock);
                        arg_value = builder.CreateExtractValue(arg_value, {0}, "arg_unwrap");
                    }
                }

                // Function-to-pointer coercion: extract method_ptr from fat pointer {ptr, ptr}
                // when passing a function reference to an extern expecting a pointer (wild ?*).
                // The fat pointer layout is {method_ptr, env_ptr} — we extract field 0.
                if (arg_value->getType() != param_type &&
                    arg_value->getType()->isStructTy() && param_type->isPointerTy()) {
                    llvm::StructType* arg_struct = llvm::cast<llvm::StructType>(arg_value->getType());
                    if (arg_struct->getNumElements() == 2 &&
                        arg_struct->getElementType(0)->isPointerTy() &&
                        arg_struct->getElementType(1)->isPointerTy()) {
                        arg_value = builder.CreateExtractValue(arg_value, 0, "func_method_ptr");
                    }
                }

                // Integer type coercion: widen/narrow integer args to match function signature
                if (arg_value->getType() != param_type &&
                    arg_value->getType()->isIntegerTy() && param_type->isIntegerTy()) {
                    unsigned argBits = arg_value->getType()->getIntegerBitWidth();
                    unsigned paramBits = param_type->getIntegerBitWidth();
                    if (argBits < paramBits)
                        arg_value = builder.CreateSExt(arg_value, param_type, "arg_sext");
                    else if (argBits > paramBits)
                        arg_value = builder.CreateTrunc(arg_value, param_type, "arg_trunc");
                }

                // Floating-point type coercion: widen/narrow float args to match function signature
                // Handles float→double, double→fp128, float→fp128, and reverse narrowing
                if (arg_value->getType() != param_type &&
                    arg_value->getType()->isFloatingPointTy() && param_type->isFloatingPointTy()) {
                    unsigned argBits = arg_value->getType()->getPrimitiveSizeInBits();
                    unsigned paramBits = param_type->getPrimitiveSizeInBits();
                    if (argBits < paramBits)
                        arg_value = builder.CreateFPExt(arg_value, param_type, "arg_fpext");
                    else if (argBits > paramBits)
                        arg_value = builder.CreateFPTrunc(arg_value, param_type, "arg_fptrunc");
                }

                // v0.2.36: dyn Trait coercion — package concrete value into fat pointer
                // When a concrete typed value (e.g., int32) is passed to a dyn Trait
                // parameter, construct { ptr_to_data, ptr_to_vtable }.
                if (func_dyn_params_ && param_type->isStructTy()) {
                    auto fn_it = func_dyn_params_->find(func_name);
                    if (fn_it != func_dyn_params_->end()) {
                        auto param_it = fn_it->second.find(static_cast<unsigned>(i));
                        if (param_it != fn_it->second.end()) {
                            const std::string& traitName = param_it->second;

                            // Determine the concrete type name from arg's Aria type
                            std::string concreteType;
                            if (i < expr->arguments.size()) {
                                ASTNode* arg_node = expr->arguments[i].get();
                                if (arg_node->type == ASTNode::NodeType::IDENTIFIER) {
                                    IdentifierExpr* ident = static_cast<IdentifierExpr*>(arg_node);
                                    auto type_it = var_aria_types.find(ident->name);
                                    if (type_it != var_aria_types.end()) {
                                        concreteType = type_it->second;
                                    }
                                }
                            }
                            // Fallback: infer from LLVM type
                            if (concreteType.empty()) {
                                llvm::Type* at = arg_value->getType();
                                if (at->isIntegerTy(32)) concreteType = "int32";
                                else if (at->isIntegerTy(64)) concreteType = "int64";
                                else if (at->isIntegerTy(16)) concreteType = "int16";
                                else if (at->isIntegerTy(8)) concreteType = "int8";
                                else if (at->isDoubleTy()) concreteType = "flt64";
                                else if (at->isFloatTy()) concreteType = "flt32";
                            }

                            if (!concreteType.empty()) {
                                std::string vtableGVName = traitName + "_vtable_" + concreteType;
                                llvm::GlobalVariable* vtableGV = module->getGlobalVariable(vtableGVName, true);
                                if (vtableGV) {
                                    // Alloca space for the concrete value
                                    llvm::AllocaInst* dataAlloca = builder.CreateAlloca(
                                        arg_value->getType(), nullptr, "dyn_data");
                                    builder.CreateStore(arg_value, dataAlloca);

                                    // Build fat pointer: { ptr data, ptr vtable }
                                    // Use the actual parameter type (named %struct.DynTraitObj)
                                    // instead of anonymous { ptr, ptr } to avoid type mismatch
                                    llvm::StructType* fatPtrTy = llvm::cast<llvm::StructType>(param_type);
                                    llvm::Value* fat = llvm::UndefValue::get(fatPtrTy);
                                    fat = builder.CreateInsertValue(fat, dataAlloca, 0, "dyn_fat.data");
                                    fat = builder.CreateInsertValue(fat, vtableGV, 1, "dyn_fat.vtable");
                                    arg_value = fat;

                                    ARIA_DBG_STREAM << "[DYN] Coerced " << concreteType
                                              << " to dyn " << traitName
                                              << " fat ptr (vtable: @" << vtableGVName << ")\n";
                                }
                            }
                        }
                    }
                }
            }

            args.push_back(arg_value);
        }
        
        // Generate the call instruction
        // Void-returning functions cannot have a named result in LLVM IR
        llvm::Value* call_result;
        if (direct_func->getReturnType()->isVoidTy()) {
            call_result = builder.CreateCall(direct_func, args);
        } else {
            call_result = builder.CreateCall(direct_func, args, "calltmp");
        }
        
        // BUG-09-001 FIX: Auto-wrap FFI pointer returns in Optional
        // Check if this is an extern function returning a pointer
        llvm::Type* return_type = direct_func->getReturnType();
        bool is_extern = direct_func->hasExternalLinkage() && direct_func->isDeclaration();
        
        if (is_extern && return_type->isPointerTy()) {
            // FFI-STRING-RETURN FIX: If this extern returns a string type,
            // wrap the raw char* into a proper AriaString struct before
            // returning. Without this, the raw char* is stored as-is
            // but later code treats it as an AriaString*, reading string data
            // bytes as struct fields (causing segfaults).
            std::string callee_name = direct_func->getName().str();
            auto ffi_ret_it = var_aria_types.find("__ffi_ret_" + callee_name);
            if (ffi_ret_it != var_aria_types.end() && ffi_ret_it->second == "string") {
                // call_result is a raw char* from C. Wrap into AriaString {ptr, i64}.
                llvm::StructType* aria_string_type = llvm::StructType::getTypeByName(context, "struct.AriaString");
                if (!aria_string_type) {
                    aria_string_type = llvm::StructType::create(context,
                        {builder.getPtrTy(), builder.getInt64Ty()}, "struct.AriaString");
                }

                // Get string length via strlen
                llvm::FunctionType* strlen_type = llvm::FunctionType::get(
                    builder.getInt64Ty(), {builder.getPtrTy()}, false);
                llvm::FunctionCallee strlen_fn = module->getOrInsertFunction("strlen", strlen_type);
                llvm::Value* str_len = builder.CreateCall(strlen_fn, {call_result}, "ffi_strlen");

                // GC-allocate AriaString struct (16 bytes: ptr + i64) to survive function returns.
                // Stack alloca would become a dangling pointer when returned through wrapper functions.
                llvm::FunctionCallee gc_alloc_callee = module->getOrInsertFunction("aria_gc_alloc",
                    llvm::FunctionType::get(builder.getPtrTy(), {builder.getInt64Ty()}, false));
                llvm::Value* struct_size = llvm::ConstantInt::get(builder.getInt64Ty(), 16);
                llvm::Value* str_heap = builder.CreateCall(gc_alloc_callee, {struct_size}, "ffi_str_gc");

                // Also GC-copy the string data so it's independent of C memory lifetime
                llvm::Value* data_size = builder.CreateAdd(str_len,
                    llvm::ConstantInt::get(builder.getInt64Ty(), 1), "ffi_data_sz"); // +1 for null terminator
                llvm::Value* data_heap = builder.CreateCall(gc_alloc_callee, {data_size}, "ffi_data_gc");
                llvm::FunctionCallee memcpy_fn = module->getOrInsertFunction("memcpy",
                    llvm::FunctionType::get(builder.getPtrTy(),
                        {builder.getPtrTy(), builder.getPtrTy(), builder.getInt64Ty()}, false));
                builder.CreateCall(memcpy_fn, {data_heap, call_result, data_size});

                // Populate the GC-allocated AriaString struct
                builder.CreateStore(data_heap,
                    builder.CreateStructGEP(aria_string_type, str_heap, 0, "ffi_str_data"));
                builder.CreateStore(str_len,
                    builder.CreateStructGEP(aria_string_type, str_heap, 1, "ffi_str_len"));

                // Use the GC-allocated AriaString pointer as the result.
                // Skip Optional wrapping — the string is always valid (never NULL).
                call_result = str_heap;
                return call_result;
            }

            // User-declared extern functions return the declared type directly.
            // Don't wrap in Optional — the user explicitly chose the return type.
            // If null safety is needed, the user should declare Optional<T> return.
            return call_result;
        }
        
        // PIPELINE OPERATOR FIX: Auto-unwrap Result<T> → T for pipeline call return values
        // When the final result of a pipeline (value |> f1 |> f2) is assigned to a T variable,
        // extract the .val field from the Result<T> return value
        if (expr->isPipelineCall && return_type->isStructTy()) {
            llvm::StructType* return_struct = llvm::cast<llvm::StructType>(return_type);
            
            // Result types are structs with 3 fields: {T val, error* err, i8 is_error}
            // Check if this looks like a Result type by field count and types
            if (return_struct->getNumElements() == 3 &&
                return_struct->getElementType(1)->isPointerTy() &&
                (return_struct->getElementType(2)->isIntegerTy(8) || return_struct->getElementType(2)->isIntegerTy(1))) {
                
                // Extract the value field (field 0) from Result
                call_result = builder.CreateExtractValue(call_result, 0, "pipeline_result_unwrap");
            }
        }
        
        return call_result;
        
    } else {
        throw std::runtime_error("Unknown function or closure: " + callee_ident->name);
    }
}

