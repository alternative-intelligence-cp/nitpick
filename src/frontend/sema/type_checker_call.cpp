// type_checker_call.cpp — Split from type_checker.cpp for parallel builds (v0.8.2)
#include "frontend/sema/type_checker.h"
#include "frontend/sema/type_checker_internal.h"
#include "frontend/sema/generic_resolver.h"
#include "frontend/sema/exhaustiveness.h"
#include "frontend/sema/definite_assignment.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/type.h"
#include "semantic/literal_converter.h"
#include <algorithm>
#include <sstream>
#include <variant>
#include <iostream>
#include <unordered_set>
#include <set>
#include <cmath>

namespace aria {
namespace sema {

using namespace tc_helpers;

// ============================================================================
// Function Call Type Inference
// ============================================================================

Type* TypeChecker::inferCallExpr(CallExpr* expr) {
    // GPU/PTX Backend Phase 4: Handle GPU intrinsic calls (gpu.thread_id(), gpu.sync_threads(), etc.)
    if (expr->callee->type == ASTNode::NodeType::MEMBER_ACCESS) {
        MemberAccessExpr* memberExpr = static_cast<MemberAccessExpr*>(expr->callee.get());
        if (memberExpr->object->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* identExpr = static_cast<IdentifierExpr*>(memberExpr->object.get());
            if (identExpr->name == "gpu") {
                // GPU intrinsics: all return int32 except sync_threads (which returns void but used as statement)
                // For simplicity, return int32 for all - sync_threads value will be ignored
                return typeSystem->getPrimitiveType("int32");
            }
        }
    }
    
    // Check for namespace-qualified static method calls: Type.method()
    // Example: string.from_char(65) -> resolves to builtin string_from_char
    // Also handles UFCS instance method calls: obj.method() -> Type_method(obj)
    // AND module function calls: module.function()
    if (expr->callee->type == ASTNode::NodeType::MEMBER_ACCESS) {
        MemberAccessExpr* memberExpr = static_cast<MemberAccessExpr*>(expr->callee.get());

        // Check if the object is an identifier (type name, module, or variable)
        if (memberExpr->object->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* identExpr = static_cast<IdentifierExpr*>(memberExpr->object.get());
            std::string name = identExpr->name;
            std::string methodName = memberExpr->member;

            // First check if this is a MODULE symbol
            Symbol* baseSymbol = symbolTable->resolveSymbol(name);
            if (baseSymbol && baseSymbol->isModule()) {
                Module* module = baseSymbol->getModuleRef();
                if (!module) {
                    addError("Module '" + name + "' has invalid module reference", expr);
                    return typeSystem->getErrorType();
                }

                // Look up function in module exports
                const auto& exports = module->getExports();
                auto it = exports.find(methodName);
                if (it == exports.end()) {
                    addError("Module '" + name + "' has no exported function '" + methodName + "'", expr);
                    return typeSystem->getErrorType();
                }

                Symbol* funcSymbol = it->second.symbol;
                if (!funcSymbol || !funcSymbol->type) {
                    addError("Invalid function symbol in module exports", expr);
                    return typeSystem->getErrorType();
                }

                // Verify it's actually a function
                if (funcSymbol->type->getKind() != TypeKind::FUNCTION) {
                    addError("'" + methodName + "' in module '" + name + "' is not a function", expr);
                    return typeSystem->getErrorType();
                }

                FunctionType* funcType = static_cast<FunctionType*>(funcSymbol->type);

                // Check argument count
                if (expr->arguments.size() != funcType->getParamTypes().size()) {
                    std::ostringstream oss;
                    oss << "Function '" + methodName + "' expects " 
                        << funcType->getParamTypes().size() << " arguments, got " 
                        << expr->arguments.size();
                    addError(oss.str(), expr);
                    return typeSystem->getErrorType();
                }

                // Type check arguments (basic type checking)
                const auto& paramTypes = funcType->getParamTypes();
                for (size_t i = 0; i < expr->arguments.size(); ++i) {
                    Type* argType = inferType(expr->arguments[i].get());
                    // Check for error type
                    if (argType->getKind() == TypeKind::ERROR) {
                        return typeSystem->getErrorType();
                    }
                    // TODO: Add proper type compatibility checking
                    // For now, just ensure no error types pass through
                }

                // Return the function's return type
                return funcType->getReturnType();
            }

            // Check if this is a type name (static method call) or a variable (UFCS)
            // Type keywords: string, int64, array, etc.
            // Also check if it's a user-defined type (struct name)
            bool isTypeName = isTypeKeyword(name) ||
                              (baseSymbol && baseSymbol->kind == SymbolKind::TYPE);

            if (isTypeName) {
                // Static method call: Type.method() -> Type_method()
                std::string builtinName = name + "_" + methodName;

                auto tempIdExpr = std::make_shared<IdentifierExpr>(builtinName, expr->line, expr->column);
                CallExpr tempCall(tempIdExpr, expr->arguments, expr->line, expr->column);
                tempCall.explicitTypeArgs = expr->explicitTypeArgs;

                return inferCallExpr(&tempCall);
            } else {
                // UFCS: obj.method() -> Type_method(obj)
                // Look up the variable to get its type
                Symbol* varSymbol = symbolTable->resolveSymbol(name);
                if (varSymbol && varSymbol->type) {
                    // Get the type name from the variable's type
                    std::string typeName = varSymbol->type->toString();

                    // Handle pointer types - strip the pointer for method lookup
                    if (!typeName.empty() && (typeName.back() == '*' || typeName.back() == '@')) {
                        typeName = typeName.substr(0, typeName.length() - 1);
                    }

                    // ====================================================================
                    // SPECIAL HANDLING: atomic<T> methods
                    // ====================================================================
                    // atomic<T> methods resolve directly without UFCS transformation
                    // Methods: load(), store(), swap(), compare_exchange(), fetch_add(), fetch_sub()
                    // Supports both "atomic<int32>" and "atomic_int32" formats
                    if (typeName.find("atomic<") == 0 || typeName.find("atomic_") == 0) {
                        // Extract inner type
                        std::string innerType;
                        if (typeName.find("atomic<") == 0) {
                            size_t start = typeName.find('<') + 1;
                            size_t end = typeName.find('>');
                            if (start != std::string::npos && end != std::string::npos) {
                                innerType = typeName.substr(start, end - start);
                            }
                        } else {
                            // atomic_int32 → int32
                            innerType = typeName.substr(7);  // Skip "atomic_"
                        }
                        
                        // Validate method name
                        std::string method = memberExpr->member;
                        if (method == "load") {
                            // load() -> T (returns inner type)
                            Type* retType = typeSystem->getPrimitiveType(innerType);
                            if (retType) return retType;
                            return typeSystem->getPrimitiveType("int32");  // fallback
                        }
                        else if (method == "store") {
                            // store(value) -> void
                            if (expr->arguments.size() != 1) {
                                addError("atomic.store() requires exactly one argument", expr);
                                return typeSystem->getErrorType();
                            }
                            Type* argType = inferType(expr->arguments[0].get());
                            if (argType->getKind() == TypeKind::ERROR) {
                                return typeSystem->getErrorType();
                            }
                            return typeSystem->getPrimitiveType("void");
                        }
                        else if (method == "swap") {
                            // swap(value) -> T (returns old value)
                            if (expr->arguments.size() != 1) {
                                addError("atomic.swap() requires exactly one argument", expr);
                                return typeSystem->getErrorType();
                            }
                            Type* argType = inferType(expr->arguments[0].get());
                            if (argType->getKind() == TypeKind::ERROR) {
                                return typeSystem->getErrorType();
                            }
                            // Return the same type as the argument
                            return argType;
                        }
                        else if (method == "fetch_add" || method == "fetch_sub") {
                            // fetch_add/sub(delta) -> T (returns old value)
                            if (expr->arguments.size() != 1) {
                                addError("atomic." + method + "() requires exactly one argument", expr);
                                return typeSystem->getErrorType();
                            }
                            Type* argType = inferType(expr->arguments[0].get());
                            if (argType->getKind() == TypeKind::ERROR) {
                                return typeSystem->getErrorType();
                            }
                            return argType;
                        }
                        else if (method == "compare_exchange") {
                            // compare_exchange(expected, desired) -> bool
                            if (expr->arguments.size() != 2) {
                                addError("atomic.compare_exchange() requires exactly two arguments", expr);
                                return typeSystem->getErrorType();
                            }
                            Type* arg1 = inferType(expr->arguments[0].get());
                            Type* arg2 = inferType(expr->arguments[1].get());
                            if (arg1->getKind() == TypeKind::ERROR || arg2->getKind() == TypeKind::ERROR) {
                                return typeSystem->getErrorType();
                            }
                            return typeSystem->getPrimitiveType("bool");
                        }
                        else {
                            addError("Unknown atomic method: " + method, expr);
                            return typeSystem->getErrorType();
                        }
                    }

                    // ====================================================================
                    // SPECIAL HANDLING: any type methods (get, set, resolve)
                    // ====================================================================
                    // any:x = ...; x.get::<int64>() -> int64
                    // any:x = ...; x.set::<int64>(42) -> void
                    // any:x = ...; x.resolve::<int64>() -> int64-> (consuming)
                    if (varSymbol->type->getKind() == TypeKind::ANY) {
                        std::string method = memberExpr->member;
                        AnyType* anyType = static_cast<AnyType*>(varSymbol->type);

                        if (method == "get") {
                            // get::<T>() — Read value as T (runtime checked)
                            if (expr->explicitTypeArgs.empty()) {
                                addError("any.get() requires a type argument: use .get::<T>()\n"
                                         "  Example: myAny.get::<int64>()", expr);
                                return typeSystem->getErrorType();
                            }
                            if (expr->arguments.size() != 0) {
                                addError("any.get::<T>() takes no arguments", expr);
                                return typeSystem->getErrorType();
                            }
                            std::string targetName = expr->explicitTypeArgs[0];
                            Type* targetType = typeSystem->getPrimitiveType(targetName);
                            if (!targetType) {
                                // Try struct lookup
                                targetType = typeSystem->getStructType(targetName);
                            }
                            if (!targetType) {
                                addError("Unknown type '" + targetName + "' in any.get::<" + targetName + ">()", expr);
                                return typeSystem->getErrorType();
                            }
                            return targetType;
                        }
                        else if (method == "set") {
                            // set::<T>(val) — Write T value (runtime checked)
                            if (expr->explicitTypeArgs.empty()) {
                                addError("any.set() requires a type argument: use .set::<T>(value)\n"
                                         "  Example: myAny.set::<int64>(42)", expr);
                                return typeSystem->getErrorType();
                            }
                            if (expr->arguments.size() != 1) {
                                addError("any.set::<T>(value) requires exactly one argument", expr);
                                return typeSystem->getErrorType();
                            }
                            std::string targetName = expr->explicitTypeArgs[0];
                            Type* targetType = typeSystem->getPrimitiveType(targetName);
                            if (!targetType) {
                                targetType = typeSystem->getStructType(targetName);
                            }
                            if (!targetType) {
                                addError("Unknown type '" + targetName + "' in any.set::<" + targetName + ">()", expr);
                                return typeSystem->getErrorType();
                            }
                            // Type check the argument against the target type
                            Type* argType = inferType(expr->arguments[0].get());
                            if (argType->getKind() == TypeKind::ERROR) {
                                return typeSystem->getErrorType();
                            }
                            if (!argType->isAssignableTo(targetType) && !canCoerce(argType, targetType)) {
                                addError("any.set::<" + targetName + ">() argument type '" + argType->toString() +
                                         "' is not compatible with '" + targetName + "'", expr);
                                return typeSystem->getErrorType();
                            }
                            return typeSystem->getPrimitiveType("void");
                        }
                        else if (method == "resolve") {
                            // resolve::<T>() — Consuming transform: any -> T-> (fat pointer)
                            if (expr->explicitTypeArgs.empty()) {
                                addError("any.resolve() requires a type argument: use .resolve::<T>()\n"
                                         "  Example: myAny.resolve::<int64>()", expr);
                                return typeSystem->getErrorType();
                            }
                            if (expr->arguments.size() != 0) {
                                addError("any.resolve::<T>() takes no arguments", expr);
                                return typeSystem->getErrorType();
                            }
                            std::string targetName = expr->explicitTypeArgs[0];
                            Type* targetType = typeSystem->getPrimitiveType(targetName);
                            if (!targetType) {
                                targetType = typeSystem->getStructType(targetName);
                            }
                            if (!targetType) {
                                addError("Unknown type '" + targetName + "' in any.resolve::<" + targetName + ">()", expr);
                                return typeSystem->getErrorType();
                            }
                            // resolve returns a fat pointer (T->) to the underlying data
                            return typeSystem->getPointerType(targetType, /*isMutable=*/false, /*isWild=*/anyType->isWildAny());
                        }
                        else {
                            addError("Unknown method '" + method + "' on any type.\n"
                                     "  Available methods: .get::<T>(), .set::<T>(value), .resolve::<T>()", expr);
                            return typeSystem->getErrorType();
                        }
                    }

                    // ====================================================================
                    // SPECIAL HANDLING: dyn Trait method calls (dynamic dispatch)
                    // ====================================================================
                    // dyn Trait:x = ...; x.method(args) -> dispatched via vtable
                    // Type checker validates method exists on trait and returns its type
                    if (typeName.substr(0, 4) == "dyn ") {
                        std::string traitName = typeName.substr(4);
                        std::string method = memberExpr->member;

                        // Look up the trait symbol
                        Symbol* traitSym = symbolTable->lookupSymbol(traitName);
                        if (!traitSym || !traitSym->isTrait()) {
                            addError("Unknown trait '" + traitName + "' in dyn dispatch", expr);
                            return typeSystem->getErrorType();
                        }

                        TraitDeclStmt* traitDecl = traitSym->getTraitDecl();
                        if (!traitDecl) {
                            addError("Trait '" + traitName + "' has no declaration", expr);
                            return typeSystem->getErrorType();
                        }

                        // Find the method in the trait's method list
                        for (const auto& tm : traitDecl->methods) {
                            if (tm.name == method) {
                                // Resolve the return type
                                Type* retType = typeSystem->getPrimitiveType(tm.returnType);
                                if (!retType) {
                                    retType = typeSystem->getStructType(tm.returnType);
                                }
                                if (!retType) {
                                    // Trait methods return Result<T> at ABI level,
                                    // but the declared return type is the inner type
                                    retType = typeSystem->getPrimitiveType("int32");  // fallback
                                }
                                return retType;
                            }
                        }

                        addError("Trait '" + traitName + "' has no method '" + method + "'", expr);
                        return typeSystem->getErrorType();
                    }

                    // Handle generic types - use base type for method lookup
                    // e.g., array<string> -> array
                    size_t anglePos = typeName.find('<');
                    if (anglePos != std::string::npos) {
                        typeName = typeName.substr(0, anglePos);
                    }

                    // Construct the UFCS function name: typename_methodname
                    std::string ufcsName = typeName + "_" + methodName;

                    // Create the UFCS call with the object as the first argument
                    std::vector<ASTNodePtr> newArgs;
                    newArgs.push_back(memberExpr->object);  // Inject object as first arg
                    for (auto& arg : expr->arguments) {
                        newArgs.push_back(arg);  // Append original arguments
                    }

                    auto tempIdExpr = std::make_shared<IdentifierExpr>(ufcsName, expr->line, expr->column);
                    CallExpr tempCall(tempIdExpr, newArgs, expr->line, expr->column);
                    tempCall.explicitTypeArgs = expr->explicitTypeArgs;

                    return inferCallExpr(&tempCall);
                }
            }
        }
    }
    
    // Check for builtin functions first
    if (expr->callee->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* idExpr = static_cast<IdentifierExpr*>(expr->callee.get());
        
        // ====================================================================
        // COMPILER INTRINSICS - Compile-Time Reflection
        // ====================================================================
        
        // @sizeof(type) -> int64 - Returns size of type in bytes
        if (idExpr->name == "@sizeof" || idExpr->name == "sizeof") {
            if (expr->arguments.size() != 1) {
                addError("@sizeof() requires exactly one argument (type or expression)", expr);
                return typeSystem->getErrorType();
            }
            // For now, just return int64 - actual value computed at compile time
            return typeSystem->getPrimitiveType("int64");
        }
        
        // @alignof(type) -> int64 - Returns alignment requirement in bytes
        if (idExpr->name == "@alignof" || idExpr->name == "alignof") {
            if (expr->arguments.size() != 1) {
                addError("@alignof() requires exactly one argument (type)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // @typeof(expr) -> type - Returns type of expression
        if (idExpr->name == "@typeof" || idExpr->name == "typeof") {
            if (expr->arguments.size() != 1) {
                addError("@typeof() requires exactly one argument (expression)", expr);
                return typeSystem->getErrorType();
            }
            // Returns the type itself (for metaprogramming)
            Type* argType = inferType(expr->arguments[0].get());
            return argType;
        }
        
        // @offsetof(type, field) -> int64 - Returns byte offset of field in struct
        if (idExpr->name == "@offsetof" || idExpr->name == "offsetof") {
            if (expr->arguments.size() != 2) {
                addError("@offsetof() requires exactly two arguments (type, field_name)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // ====================================================================
        // COMPILER HINTS - Branch Prediction & Performance
        // ====================================================================
        
        // @likely(bool) -> bool - Hints that condition is usually true
        if (idExpr->name == "@likely" || idExpr->name == "likely") {
            if (expr->arguments.size() != 1) {
                addError("@likely() requires exactly one argument (bool condition)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // @unlikely(bool) -> bool - Hints that condition is usually false
        if (idExpr->name == "@unlikely" || idExpr->name == "unlikely") {
            if (expr->arguments.size() != 1) {
                addError("@unlikely() requires exactly one argument (bool condition)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // @prefetch(ptr) -> void - Prefetch data into cache
        if (idExpr->name == "@prefetch" || idExpr->name == "prefetch") {
            if (expr->arguments.size() != 1) {
                addError("@prefetch() requires exactly one argument (pointer)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }
        
        // ====================================================================
        // DEBUG & ASSERTIONS
        // ====================================================================
        
        // @assert(condition) -> void - Runtime assertion
        if (idExpr->name == "@assert" || idExpr->name == "assert") {
            if (expr->arguments.size() < 1 || expr->arguments.size() > 2) {
                addError("@assert() requires 1-2 arguments (condition, optional message)", expr);
                return typeSystem->getErrorType();
            }
            Type* condType = inferType(expr->arguments[0].get());
            if (condType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }
        
        // @unreachable() -> never - Marks code path as impossible
        if (idExpr->name == "@unreachable" || idExpr->name == "unreachable") {
            if (expr->arguments.size() != 0) {
                addError("@unreachable() takes no arguments", expr);
                return typeSystem->getErrorType();
            }
            // Return 'never' type (for now using error type as placeholder)
            return typeSystem->getErrorType();
        }
        
        // ====================================================================
        // SIMD HORIZONTAL REDUCTIONS (P1-2 Phase 5)
        // ====================================================================
        // Reduce SIMD vector to scalar value
        
        // @simd_sum(simd<T, N>) -> T - Sum all vector elements
        if (idExpr->name == "@simd_sum" || idExpr->name == "simd_sum") {
            if (expr->arguments.size() != 1) {
                addError("@simd_sum() requires exactly one SIMD vector argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->getKind() != TypeKind::SIMD) {
                addError("@simd_sum() requires SIMD vector argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            // Return element type: simd<T, N> -> T
            SimdType* simdType = static_cast<SimdType*>(argType);
            return simdType->getElementType();
        }
        
        // @simd_product(simd<T, N>) -> T - Multiply all vector elements
        if (idExpr->name == "@simd_product" || idExpr->name == "simd_product") {
            if (expr->arguments.size() != 1) {
                addError("@simd_product() requires exactly one SIMD vector argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->getKind() != TypeKind::SIMD) {
                addError("@simd_product() requires SIMD vector argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            SimdType* simdType = static_cast<SimdType*>(argType);
            return simdType->getElementType();
        }
        
        // @simd_min(simd<T, N>) -> T - Find minimum element
        if (idExpr->name == "@simd_min" || idExpr->name == "simd_min") {
            if (expr->arguments.size() != 1) {
                addError("@simd_min() requires exactly one SIMD vector argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->getKind() != TypeKind::SIMD) {
                addError("@simd_min() requires SIMD vector argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            SimdType* simdType = static_cast<SimdType*>(argType);
            return simdType->getElementType();
        }
        
        // @simd_max(simd<T, N>) -> T - Find maximum element
        if (idExpr->name == "@simd_max" || idExpr->name == "simd_max") {
            if (expr->arguments.size() != 1) {
                addError("@simd_max() requires exactly one SIMD vector argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->getKind() != TypeKind::SIMD) {
                addError("@simd_max() requires SIMD vector argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            SimdType* simdType = static_cast<SimdType*>(argType);
            return simdType->getElementType();
        }
        
        // ====================================================================
        // SIMD BOOLEAN OPERATIONS (P1-2 Phase 6)
        // ====================================================================
        // Boolean reductions for SIMD mask operations
        
        // @simd_any(simd<bool, N>) -> bool - True if ANY lane is true
        if (idExpr->name == "@simd_any" || idExpr->name == "simd_any") {
            if (expr->arguments.size() != 1) {
                addError("simd_any() requires exactly one SIMD boolean vector argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->getKind() != TypeKind::SIMD) {
                addError("simd_any() requires SIMD vector argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            // Validate element type is bool
            SimdType* simdType = static_cast<SimdType*>(argType);
            Type* elemType = simdType->getElementType();
            if (elemType->toString() != "bool") {
                addError("simd_any() requires simd<bool, N> argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // @simd_all(simd<bool, N>) -> bool - True if ALL lanes are true
        if (idExpr->name == "@simd_all" || idExpr->name == "simd_all") {
            if (expr->arguments.size() != 1) {
                addError("simd_all() requires exactly one SIMD boolean vector argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->getKind() != TypeKind::SIMD) {
                addError("simd_all() requires SIMD vector argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            // Validate element type is bool
            SimdType* simdType = static_cast<SimdType*>(argType);
            Type* elemType = simdType->getElementType();
            if (elemType->toString() != "bool") {
                addError("simd_all() requires simd<bool, N> argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // @simd_select(simd<bool, N>, simd<T, N>, simd<T, N>) -> simd<T, N>
        // SIMD conditional select: for each lane, pick from true_vals or false_vals based on mask
        // Equivalent to: mask ? true_vals : false_vals (per-lane)
        if (idExpr->name == "@simd_select" || idExpr->name == "simd_select") {
            if (expr->arguments.size() != 3) {
                addError("simd_select() requires exactly three arguments (mask, true_vals, false_vals)", expr);
                return typeSystem->getErrorType();
            }
            
            Type* maskType = inferType(expr->arguments[0].get());
            Type* trueType = inferType(expr->arguments[1].get());
            Type* falseType = inferType(expr->arguments[2].get());
            
            if (maskType->getKind() == TypeKind::ERROR || 
                trueType->getKind() == TypeKind::ERROR ||
                falseType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Validate mask is simd<bool, N>
            if (maskType->getKind() != TypeKind::SIMD) {
                addError("simd_select() first argument must be SIMD boolean mask, got '" + maskType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            SimdType* maskSimd = static_cast<SimdType*>(maskType);
            if (maskSimd->getElementType()->toString() != "bool") {
                addError("simd_select() mask must be simd<bool, N>, got '" + maskType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            // Validate true_vals and false_vals are SIMD with same type and lane count
            if (trueType->getKind() != TypeKind::SIMD || falseType->getKind() != TypeKind::SIMD) {
                addError("simd_select() second and third arguments must be SIMD vectors", expr);
                return typeSystem->getErrorType();
            }
            
            SimdType* trueSimd = static_cast<SimdType*>(trueType);
            SimdType* falseSimd = static_cast<SimdType*>(falseType);
            
            // Check lane counts match
            if (maskSimd->getLaneCount() != trueSimd->getLaneCount() ||
                trueSimd->getLaneCount() != falseSimd->getLaneCount()) {
                addError("simd_select() all vectors must have same lane count", expr);
                return typeSystem->getErrorType();
            }
            
            // Check true and false element types match
            if (!trueSimd->getElementType()->equals(falseSimd->getElementType())) {
                addError("simd_select() true and false vectors must have same element type", expr);
                return typeSystem->getErrorType();
            }
            
            // Return the SIMD type of true/false values
            return trueType;
        }
        
        // @simd_broadcast(T:value, int32:lanes) -> simd<T, lanes>
        // Create a SIMD vector with all lanes set to the same scalar value
        if (idExpr->name == "@simd_broadcast" || idExpr->name == "simd_broadcast") {
            if (expr->arguments.size() != 2) {
                addError("simd_broadcast() requires exactly two arguments (value, lane_count)", expr);
                return typeSystem->getErrorType();
            }
            
            Type* scalarType = inferType(expr->arguments[0].get());
            Type* laneCountType = inferType(expr->arguments[1].get());
            
            if (scalarType->getKind() == TypeKind::ERROR || laneCountType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Validate scalar value is a primitive numeric type
            if (!scalarType->isPrimitive() || !isNumericType(scalarType)) {
                addError("simd_broadcast() first argument must be a numeric scalar, got '" + scalarType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            // Validate lane count is an integer
            if (!laneCountType->isPrimitive()) {
                addError("simd_broadcast() second argument must be an integer lane count", expr);
                return typeSystem->getErrorType();
            }
            
            PrimitiveType* laneCountPrim = static_cast<PrimitiveType*>(laneCountType);
            const std::string& lcName = laneCountPrim->getName();
            if (lcName.find("int") != 0 && lcName.find("uint") != 0) {
                addError("simd_broadcast() lane count must be an integer type, got '" + lcName + "'", expr);
                return typeSystem->getErrorType();
            }
            
            // Extract lane count value (must be a compile-time constant)
            // For now, we'll accept any integer expression and validate at runtime
            // In the future, we could require a literal and extract its value
            
            // For type checking, we need to determine the lane count
            // Check if it's an integer literal
            size_t laneCount = 0;
            if (expr->arguments[1]->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* litNode = static_cast<LiteralExpr*>(expr->arguments[1].get());
                if (std::holds_alternative<int64_t>(litNode->value)) {
                    laneCount = std::get<int64_t>(litNode->value);
                } else {
                    addError("simd_broadcast() lane count must be an integer literal", expr);
                    return typeSystem->getErrorType();
                }
            } else {
                addError("simd_broadcast() lane count must be a compile-time integer literal", expr);
                return typeSystem->getErrorType();
            }
            
            // Validate lane count is reasonable
            if (laneCount == 0 || laneCount > 64) {
                addError("simd_broadcast() lane count must be between 1 and 64, got: " + std::to_string(laneCount), expr);
                return typeSystem->getErrorType();
            }
            
            // Create and return SIMD type
            return typeSystem->getSimdType(scalarType, laneCount);
        }
        
        // @simd_load(T*:ptr, int32:lanes) -> simd<T, lanes>
        // Load a SIMD vector from memory (aligned load)
        if (idExpr->name == "@simd_load" || idExpr->name == "simd_load") {
            if (expr->arguments.size() != 2) {
                addError("simd_load() requires exactly two arguments (pointer, lane_count)", expr);
                return typeSystem->getErrorType();
            }
            
            Type* ptrType = inferType(expr->arguments[0].get());
            Type* laneCountType = inferType(expr->arguments[1].get());
            
            if (ptrType->getKind() == TypeKind::ERROR || laneCountType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Validate first argument is a pointer
            if (ptrType->getKind() != TypeKind::POINTER) {
                addError("simd_load() first argument must be a pointer, got '" + ptrType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            PointerType* ptr = static_cast<PointerType*>(ptrType);
            Type* elementType = ptr->getPointeeType();
            
            // Validate element type is numeric
            if (!elementType->isPrimitive() || !isNumericType(elementType)) {
                addError("simd_load() pointer must point to numeric type, got '" + elementType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            // Extract lane count (same validation as simd_broadcast)
            size_t laneCount = 0;
            if (expr->arguments[1]->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* litNode = static_cast<LiteralExpr*>(expr->arguments[1].get());
                if (std::holds_alternative<int64_t>(litNode->value)) {
                    laneCount = std::get<int64_t>(litNode->value);
                } else {
                    addError("simd_load() lane count must be an integer literal", expr);
                    return typeSystem->getErrorType();
                }
            } else {
                addError("simd_load() lane count must be a compile-time integer literal", expr);
                return typeSystem->getErrorType();
            }
            
            // Validate lane count
            if (laneCount == 0 || laneCount > 64) {
                addError("simd_load() lane count must be between 1 and 64, got: " + std::to_string(laneCount), expr);
                return typeSystem->getErrorType();
            }
            
            // Return simd<T, lanes> where T is the pointed-to type
            return typeSystem->getSimdType(elementType, laneCount);
        }
        
        // @simd_store(T*:ptr, simd<T, N>:vec) -> void
        // Store a SIMD vector to memory (aligned store)
        if (idExpr->name == "@simd_store" || idExpr->name == "simd_store") {
            if (expr->arguments.size() != 2) {
                addError("simd_store() requires exactly two arguments (pointer, vector)", expr);
                return typeSystem->getErrorType();
            }
            
            Type* ptrType = inferType(expr->arguments[0].get());
            Type* vecType = inferType(expr->arguments[1].get());
            
            if (ptrType->getKind() == TypeKind::ERROR || vecType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Validate first argument is a pointer
            if (ptrType->getKind() != TypeKind::POINTER) {
                addError("simd_store() first argument must be a pointer, got '" + ptrType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            // Validate second argument is a SIMD vector
            if (vecType->getKind() != TypeKind::SIMD) {
                addError("simd_store() second argument must be a SIMD vector, got '" + vecType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            PointerType* ptr = static_cast<PointerType*>(ptrType);
            SimdType* vec = static_cast<SimdType*>(vecType);
            
            Type* ptrElementType = ptr->getPointeeType();
            Type* vecElementType = vec->getElementType();
            
            // Validate pointer element type matches SIMD element type
            if (!ptrElementType->equals(vecElementType)) {
                addError("simd_store() pointer element type must match SIMD element type, got pointer to '" + 
                        ptrElementType->toString() + "' and SIMD of '" + vecElementType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            // Return void
            return typeSystem->getPrimitiveType("void");
        }
        
        // @breakpoint() -> void - Insert debugger breakpoint
        if (idExpr->name == "@breakpoint" || idExpr->name == "breakpoint") {
            if (expr->arguments.size() != 0) {
                addError("@breakpoint() takes no arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }
        
        // @trap() -> never - Trigger intentional trap/abort
        if (idExpr->name == "@trap" || idExpr->name == "trap") {
            if (expr->arguments.size() != 0) {
                addError("@trap() takes no arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }
        
        // ====================================================================
        // BIT MANIPULATION INTRINSICS
        // ====================================================================
        
        // @bswap16/32/64(value) -> same - Byte swap
        if (idExpr->name == "@bswap16" || idExpr->name == "bswap16") {
            if (expr->arguments.size() != 1) {
                addError("@bswap16() requires exactly one argument (int16)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int16");
        }
        if (idExpr->name == "@bswap32" || idExpr->name == "bswap32") {
            if (expr->arguments.size() != 1) {
                addError("@bswap32() requires exactly one argument (int32)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        if (idExpr->name == "@bswap64" || idExpr->name == "bswap64") {
            if (expr->arguments.size() != 1) {
                addError("@bswap64() requires exactly one argument (int64)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // @clz32/64(value) -> int32 - Count leading zeros
        if (idExpr->name == "@clz32" || idExpr->name == "clz32") {
            if (expr->arguments.size() != 1) {
                addError("@clz32() requires exactly one argument (int32)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        if (idExpr->name == "@clz64" || idExpr->name == "clz64") {
            if (expr->arguments.size() != 1) {
                addError("@clz64() requires exactly one argument (int64)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        // @ctz32/64(value) -> int32 - Count trailing zeros
        if (idExpr->name == "@ctz32" || idExpr->name == "ctz32") {
            if (expr->arguments.size() != 1) {
                addError("@ctz32() requires exactly one argument (int32)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        if (idExpr->name == "@ctz64" || idExpr->name == "ctz64") {
            if (expr->arguments.size() != 1) {
                addError("@ctz64() requires exactly one argument (int64)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        // @popcount32/64(value) -> int32 - Count set bits
        if (idExpr->name == "@popcount32" || idExpr->name == "popcount32") {
            if (expr->arguments.size() != 1) {
                addError("@popcount32() requires exactly one argument (int32)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        if (idExpr->name == "@pop count64" || idExpr->name == "popcount64") {
            if (expr->arguments.size() != 1) {
                addError("@popcount64() requires exactly one argument (int64)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        // ====================================================================
        // FRAC (EXACT RATIONAL) ARITHMETIC INTRINSICS
        // ====================================================================
        // Mixed-number fractions: frac32 = {tbb32 whole, tbb32 num, tbb32 denom}
        // Exact rational arithmetic with automatic GCD reduction
        
        // frac*_from_parts(whole, num, denom) -> frac*
        // Construct fraction from components (validates denom != 0)
        for (const char* width : {"8", "16", "32", "64"}) {
            std::string funcName = "frac" + std::string(width) + "_from_parts";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 3) {
                    addError(funcName + "() requires exactly 3 arguments (whole, numerator, denominator)", expr);
                    return typeSystem->getErrorType();
                }
                // Validate all arguments are integers (will be converted to tbb internally)
                for (size_t i = 0; i < 3; i++) {
                    Type* argType = inferType(expr->arguments[i].get());
                    if (!argType->isPrimitive() || !isNumericType(argType)) {
                        addError(funcName + "() arguments must be integer values", expr);
                        return typeSystem->getErrorType();
                    }
                }
                return typeSystem->getPrimitiveType("frac" + std::string(width));
            }
        }
        
        // frac*_add/sub/mul/div(a, b) -> frac*
        // Arithmetic operations with automatic GCD reduction
        for (const char* width : {"8", "16", "32", "64"}) {
            std::string frac_type = "frac" + std::string(width);
            for (const char* op : {"add", "sub", "mul", "div"}) {
                std::string funcName = frac_type + "_" + op;
                if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                    if (expr->arguments.size() != 2) {
                        addError(funcName + "() requires exactly 2 arguments", expr);
                        return typeSystem->getErrorType();
                    }
                    // Validate both arguments are the same frac type
                    Type* arg0Type = inferType(expr->arguments[0].get());
                    Type* arg1Type = inferType(expr->arguments[1].get());
                    
                    if (arg0Type->toString() != frac_type || arg1Type->toString() != frac_type) {
                        addError(funcName + "() requires two " + frac_type + " arguments", expr);
                        return typeSystem->getErrorType();
                    }
                    return typeSystem->getPrimitiveType(frac_type);
                }
            }
        }
        
        // frac*_neg(a) -> frac*
        // Negation
        for (const char* width : {"8", "16", "32", "64"}) {
            std::string frac_type = "frac" + std::string(width);
            std::string funcName = frac_type + "_neg";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 1) {
                    addError(funcName + "() requires exactly 1 argument", expr);
                    return typeSystem->getErrorType();
                }
                Type* argType = inferType(expr->arguments[0].get());
                if (argType->toString() != frac_type) {
                    addError(funcName + "() requires a " + frac_type + " argument", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType(frac_type);
            }
        }
        
        // frac*_cmp(a, b) -> int32
        // Comparison: returns -1 (a < b), 0 (a == b), +1 (a > b)
        for (const char* width : {"8", "16", "32", "64"}) {
            std::string frac_type = "frac" + std::string(width);
            std::string funcName = frac_type + "_cmp";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 2) {
                    addError(funcName + "() requires exactly 2 arguments", expr);
                    return typeSystem->getErrorType();
                }
                Type* arg0Type = inferType(expr->arguments[0].get());
                Type* arg1Type = inferType(expr->arguments[1].get());
                
                if (arg0Type->toString() != frac_type || arg1Type->toString() != frac_type) {
                    addError(funcName + "() requires two " + frac_type + " arguments", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType("int32");
            }
        }
        
        // frac*_to_int(a) -> int32/int64
        // Convert fraction to integer (truncates towards zero)
        for (const char* width : {"8", "16", "32", "64"}) {
            std::string frac_type = "frac" + std::string(width);
            std::string funcName = frac_type + "_to_int";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 1) {
                    addError(funcName + "() requires exactly 1 argument", expr);
                    return typeSystem->getErrorType();
                }
                Type* argType = inferType(expr->arguments[0].get());
                if (argType->toString() != frac_type) {
                    addError(funcName + "() requires a " + frac_type + " argument", expr);
                    return typeSystem->getErrorType();
                }
                // Return int32 for frac8/16/32, int64 for frac64
                if (std::string(width) == "64") {
                    return typeSystem->getPrimitiveType("int64");
                } else {
                    return typeSystem->getPrimitiveType("int32");
                }
            }
        }
        
        // frac*_to_float(a) -> float32/float64
        // Convert fraction to floating point
        for (const char* width : {"8", "16", "32", "64"}) {
            std::string frac_type = "frac" + std::string(width);
            std::string funcName = frac_type + "_to_float";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 1) {
                    addError(funcName + "() requires exactly 1 argument", expr);
                    return typeSystem->getErrorType();
                }
                Type* argType = inferType(expr->arguments[0].get());
                if (argType->toString() != frac_type) {
                    addError(funcName + "() requires a " + frac_type + " argument", expr);
                    return typeSystem->getErrorType();
                }
                // Return flt32 for frac8/16/32, flt64 for frac64
                if (std::string(width) == "64") {
                    return typeSystem->getPrimitiveType("flt64");
                } else {
                    return typeSystem->getPrimitiveType("flt32");
                }
            }
        }
        
        // ====================================================================
        // TFP (TWISTED FLOATING POINT) INTRINSICS
        // ====================================================================
        // Deterministic cross-platform floating-point with TBB safety model
        // tfp32: {tbb16 exp, tbb16 mant}
        // tfp64: {tbb16 exp, tbb48 mant}
        
        // tfp*_from_parts(exp, mant) -> tfp*
        for (const char* width : {"32", "64"}) {
            std::string funcName = "tfp" + std::string(width) + "_from_parts";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 2) {
                    addError(funcName + "() requires exactly 2 arguments (exponent, mantissa)", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType("tfp" + std::string(width));
            }
        }
        
        // tfp*_from_double(double) -> tfp*
        for (const char* width : {"32", "64"}) {
            std::string funcName = "tfp" + std::string(width) + "_from_double";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 1) {
                    addError(funcName + "() requires exactly 1 argument (double value)", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType("tfp" + std::string(width));
            }
        }
        
        // tfp*_to_double(tfp*) -> flt64
        for (const char* width : {"32", "64"}) {
            std::string funcName = "tfp" + std::string(width) + "_to_double";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 1) {
                    addError(funcName + "() requires exactly 1 argument (tfp value)", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType("flt64");
            }
        }
        
        // tfp* arithmetic: add, sub, mul, div
        for (const char* width : {"32", "64"}) {
            for (const char* op : {"add", "sub", "mul", "div"}) {
                std::string funcName = "tfp" + std::string(width) + "_" + op;
                if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                    if (expr->arguments.size() != 2) {
                        addError(funcName + "() requires exactly 2 arguments", expr);
                        return typeSystem->getErrorType();
                    }
                    return typeSystem->getPrimitiveType("tfp" + std::string(width));
                }
            }
        }
        
        // tfp*_neg(tfp*) -> tfp*
        for (const char* width : {"32", "64"}) {
            std::string funcName = "tfp" + std::string(width) + "_neg";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 1) {
                    addError(funcName + "() requires exactly 1 argument", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType("tfp" + std::string(width));
            }
        }
        
        // tfp*_cmp(tfp*, tfp*) -> int32
        for (const char* width : {"32", "64"}) {
            std::string funcName = "tfp" + std::string(width) + "_cmp";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 2) {
                    addError(funcName + "() requires exactly 2 arguments", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType("int32");
            }           
        }
        
        // tfp* math: sqrt, sin, cos, exp, log (single argument)
        for (const char* width : {"32", "64"}) {
            for (const char* mathfunc : {"sqrt", "sin", "cos", "exp", "log"}) {
                std::string funcName = "tfp" + std::string(width) + "_" + mathfunc;
                if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                    if (expr->arguments.size() != 1) {
                        addError(funcName + "() requires exactly 1 argument", expr);
                        return typeSystem->getErrorType();
                    }
                    return typeSystem->getPrimitiveType("tfp" + std::string(width));
                }
            }
        }
        
        // tfp*_pow(base, exp) -> tfp*
        for (const char* width : {"32", "64"}) {
            std::string funcName = "tfp" + std::string(width) + "_pow";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 2) {
                    addError(funcName + "() requires exactly 2 arguments (base, exponent)", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType("tfp" + std::string(width));
            }
        }
        
        // ====================================================================
        // UNKNOWN STATE MANAGEMENT (Phase 5.2)
        // ====================================================================
        
        // ok(value) -> value - Strips "unknown taint", acknowledges unknown state
        // Purpose: Force explicit acknowledgment when propagating unknown values
        // Example: int32:y = ok(x);  // "I know this might be unknown, that's OK"
        if (idExpr->name == "ok") {
            if (expr->arguments.size() != 1) {
                addError("'ok' requires exactly one argument (value to acknowledge)", expr);
                return typeSystem->getErrorType();
            }
            // Infer type of argument
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // ok() is a pass-through - returns same type as argument
            // The "taint stripping" is handled in assignment checking
            return argType;
        }
        
        // ====================================================================
        // STANDARD I/O BUILTINS
        // ====================================================================
        
        // Builtin: print() - write string as-is (no newline)
        // Builtin: println() - write string + newline
        if (idExpr->name == "print" || idExpr->name == "println") {
            if (expr->arguments.size() != 1) {
                addError(idExpr->name + "() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            // Infer argument type to validate it
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // print/println return i64 (bytes written from write() syscall)
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Builtin: stdout_write(string) -> int64
        if (idExpr->name == "stdout_write") {
            if (expr->arguments.size() != 1) {
                addError("stdout_write() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Should be string type
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Builtin: stderr_write(string) -> int64
        if (idExpr->name == "stderr_write") {
            if (expr->arguments.size() != 1) {
                addError("stderr_write() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Builtin: stddbg_write(string) -> int64
        if (idExpr->name == "stddbg_write") {
            if (expr->arguments.size() != 1) {
                addError("stddbg_write() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Builtin: stdin_read_line() -> string
        if (idExpr->name == "stdin_read_line") {
            if (expr->arguments.size() != 0) {
                addError("stdin_read_line() takes no arguments", expr);
                return typeSystem->getErrorType();
            }
            // Returns string (char*)
            return typeSystem->getPrimitiveType("string");
        }

        // Builtin: stdin_read_all() -> string
        if (idExpr->name == "stdin_read_all") {
            if (expr->arguments.size() != 0) {
                addError("stdin_read_all() takes no arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }
        
        // ====================================================================
        // SYSCALL BUILTINS — sys() / sys!!() / sys!!!()
        // Tiered system call interface (v0.4.0)
        // ====================================================================
        
        if (idExpr->name == "sys" || idExpr->name == "sys!!" || idExpr->name == "sys!!!") {
            bool isSafe = (idExpr->name == "sys");
            bool isRaw  = (idExpr->name == "sys!!!");
            
            // Need at least 1 argument (the syscall number)
            if (expr->arguments.size() < 1) {
                addError(idExpr->name + "() requires at least one argument (syscall constant)", expr);
                return typeSystem->getErrorType();
            }
            
            // Maximum 7 arguments (syscall number + 6 args)
            if (expr->arguments.size() > 7) {
                addError(idExpr->name + "() accepts at most 7 arguments (syscall number + 6 args), got " +
                         std::to_string(expr->arguments.size()), expr);
                return typeSystem->getErrorType();
            }
            
            // --- Typed return categories for sys() ---
            // Most syscalls return int64 (count, fd, 0/-errno).
            // STRING_BUF syscalls write into a user buffer and return length;
            // the compiler auto-allocates the buffer and converts to Aria string.
            enum class SysReturnCategory { INT64, STRING_BUF };
            static const std::unordered_map<std::string, SysReturnCategory> syscallReturnTypes = {
                {"GETCWD",   SysReturnCategory::STRING_BUF},
                {"READLINK", SysReturnCategory::STRING_BUF},
            };
            
            // Typed-return syscalls: validate arg count (v0.4.1)
            // STRING_BUF syscalls manage their own buffer — user provides fewer args.
            {
                IdentifierExpr* firstId = dynamic_cast<IdentifierExpr*>(expr->arguments[0].get());
                if (firstId) {
                    auto retIt = syscallReturnTypes.find(firstId->name);
                    if (retIt != syscallReturnTypes.end() && retIt->second == SysReturnCategory::STRING_BUF) {
                        if (firstId->name == "GETCWD" && expr->arguments.size() != 1) {
                            addError("sys(GETCWD) takes no additional arguments — buffer is managed automatically", expr);
                            return typeSystem->getErrorType();
                        }
                        if (firstId->name == "READLINK" && expr->arguments.size() != 2) {
                            addError("sys(READLINK, path) takes exactly one argument (the symlink path)", expr);
                            return typeSystem->getErrorType();
                        }
                    }
                }
            }
            
            // --- Safe-tier whitelist (sys() only) ---
            // The safe tier requires the first argument to be a compile-time
            // constant identifier from the curated safe syscall set.
            static const std::unordered_set<std::string> safeSyscalls = {
                // File I/O
                "READ", "WRITE", "OPEN", "CLOSE", "LSEEK", "PREAD64", "PWRITE64",
                "OPENAT", "READV", "WRITEV", "FSTAT", "NEWFSTATAT",
                // Memory
                "MMAP", "MUNMAP", "MPROTECT", "BRK", "MREMAP",
                // Directory
                "GETDENTS64", "GETCWD", "CHDIR", "MKDIR", "RMDIR", "RENAME", "RENAMEAT2",
                "UNLINK", "UNLINKAT", "SYMLINK", "READLINK",
                // File metadata
                "FCHMOD", "FCHOWN", "UTIMENSAT", "FACCESSAT",
                // Process info (read-only)
                "GETPID", "GETPPID", "GETTID", "GETUID", "GETGID", "GETEUID", "GETEGID",
                // Time
                "CLOCK_GETTIME", "CLOCK_NANOSLEEP", "NANOSLEEP",
                // Networking
                "SOCKET", "BIND", "LISTEN", "ACCEPT4", "CONNECT", "SEND", "RECV",
                "SENDTO", "RECVFROM", "SETSOCKOPT", "GETSOCKOPT", "SHUTDOWN",
                // Polling
                "POLL", "PPOLL", "EPOLL_CREATE1", "EPOLL_CTL", "EPOLL_WAIT",
                "SELECT", "PSELECT6",
                // Pipe / IPC
                "PIPE2", "DUP", "DUP2", "DUP3", "EVENTFD2",
                // Misc safe
                "IOCTL", "FCNTL", "FLOCK", "FSYNC", "FDATASYNC",
                "GETRANDOM"
            };
            
            // All known syscall constants (for full/raw tiers)
            static const std::unordered_set<std::string> allSyscalls = {
                // Everything in safeSyscalls plus dangerous ops:
                "READ", "WRITE", "OPEN", "CLOSE", "LSEEK", "PREAD64", "PWRITE64",
                "OPENAT", "READV", "WRITEV", "FSTAT", "NEWFSTATAT",
                "MMAP", "MUNMAP", "MPROTECT", "BRK", "MREMAP",
                "GETDENTS64", "GETCWD", "CHDIR", "MKDIR", "RMDIR", "RENAME", "RENAMEAT2",
                "UNLINK", "UNLINKAT", "SYMLINK", "READLINK",
                "FCHMOD", "FCHOWN", "UTIMENSAT", "FACCESSAT",
                "GETPID", "GETPPID", "GETTID", "GETUID", "GETGID", "GETEUID", "GETEGID",
                "CLOCK_GETTIME", "CLOCK_NANOSLEEP", "NANOSLEEP",
                "SOCKET", "BIND", "LISTEN", "ACCEPT4", "CONNECT", "SEND", "RECV",
                "SENDTO", "RECVFROM", "SETSOCKOPT", "GETSOCKOPT", "SHUTDOWN",
                "POLL", "PPOLL", "EPOLL_CREATE1", "EPOLL_CTL", "EPOLL_WAIT",
                "SELECT", "PSELECT6",
                "PIPE2", "DUP", "DUP2", "DUP3", "EVENTFD2",
                "IOCTL", "FCNTL", "FLOCK", "FSYNC", "FDATASYNC",
                "GETRANDOM",
                // Dangerous — require sys!! or sys!!!
                "EXIT", "EXIT_GROUP",
                "KILL", "TKILL", "TGKILL",
                "EXECVE", "EXECVEAT",
                "FORK", "CLONE", "CLONE3", "VFORK",
                "PTRACE",
                "MOUNT", "UMOUNT2",
                "REBOOT",
                "SETUID", "SETGID", "SETREUID", "SETREGID", "SETEUID", "SETEGID",
                "INIT_MODULE", "DELETE_MODULE", "FINIT_MODULE",
                "SETXATTR", "GETXATTR", "REMOVEXATTR", "LSETXATTR", "LGETXATTR", "LREMOVEXATTR",
                "FSETXATTR", "FGETXATTR", "FREMOVEXATTR", "LISTXATTR", "LLISTXATTR", "FLISTXATTR",
                "WAIT4", "WAITID",
                "PRCTL", "ARCH_PRCTL",
                "SECCOMP", "USERFAULTFD", "PERF_EVENT_OPEN",
                "SIGNALFD4", "TIMERFD_CREATE", "TIMERFD_SETTIME", "TIMERFD_GETTIME",
                "RT_SIGACTION", "RT_SIGPROCMASK", "RT_SIGRETURN", "RT_SIGPENDING",
                "SIGALTSTACK",
                "MADVISE", "MINCORE", "MLOCK", "MUNLOCK", "MLOCKALL", "MUNLOCKALL",
                "SENDMSG", "RECVMSG", "SENDMMSG", "RECVMMSG",
                "STAT", "LSTAT",
                "ACCESS", "TRUNCATE", "FTRUNCATE",
                "CHOWN", "LCHOWN", "CHMOD",
                "LINK", "LINKAT", "SYMLINKAT", "READLINKAT",
                "MKDIRAT", "MKNODAT", "UNLINKAT2",
                "STATX",
                "IO_URING_SETUP", "IO_URING_ENTER", "IO_URING_REGISTER",
                "SCHED_YIELD", "SCHED_GETAFFINITY", "SCHED_SETAFFINITY",
                "FUTEX",
                "SET_TID_ADDRESS", "SET_ROBUST_LIST", "GET_ROBUST_LIST",
                "SYSINFO", "UNAME",
                "GETRLIMIT", "SETRLIMIT", "PRLIMIT64",
                "GETRUSAGE",
                "SYSLOG",
                "INOTIFY_INIT1", "INOTIFY_ADD_WATCH", "INOTIFY_RM_WATCH"
            };
            
            // Validate first argument (syscall constant)
            if (isSafe) {
                // Safe tier: first arg MUST be an identifier from the whitelist
                IdentifierExpr* syscallId = dynamic_cast<IdentifierExpr*>(expr->arguments[0].get());
                if (!syscallId) {
                    addError("sys() requires a named syscall constant as the first argument "
                             "(e.g., READ, WRITE, OPEN). Variables and expressions are not allowed. "
                             "Use sys!!() or sys!!!() for unrestricted syscall access.", expr);
                    return typeSystem->getErrorType();
                }
                if (safeSyscalls.find(syscallId->name) == safeSyscalls.end()) {
                    if (allSyscalls.find(syscallId->name) != allSyscalls.end()) {
                        addError("'" + syscallId->name + "' is not in the safe syscall set. "
                                 "This syscall could compromise process integrity. "
                                 "Use sys!!('" + syscallId->name + "', ...) for full access, or "
                                 "sys!!!('" + syscallId->name + "', ...) for raw access.", expr);
                    } else {
                        addError("Unknown syscall constant '" + syscallId->name + "' in sys(). "
                                 "See stdlib/sys.aria for available syscall names.", expr);
                    }
                    return typeSystem->getErrorType();
                }
            }
            
            // Infer types for all arguments
            for (size_t i = 0; i < expr->arguments.size(); i++) {
                // First argument (syscall number): check for named constant FIRST
                // Named constants (READ, WRITE, etc.) are compiler builtins —
                // they are NOT variables and must NOT go through inferType().
                if (i == 0) {
                    IdentifierExpr* asId = dynamic_cast<IdentifierExpr*>(expr->arguments[0].get());
                    if (asId && (safeSyscalls.count(asId->name) || allSyscalls.count(asId->name))) {
                        // Named syscall constant — validated, codegen resolves to number
                        continue;
                    }
                    // Not a named constant — must be a numeric expression
                    if (!isSafe) {
                        // Full/raw tiers allow any integer expression as syscall number
                        Type* argType = inferType(expr->arguments[0].get());
                        if (argType->getKind() == TypeKind::ERROR) {
                            return typeSystem->getErrorType();
                        }
                        PrimitiveType* prim = dynamic_cast<PrimitiveType*>(argType);
                        if (!prim || (prim->getName() != "int64" && prim->getName() != "int32" &&
                                      prim->getName() != "uint64" && prim->getName() != "uint32")) {
                            addError(idExpr->name + "() first argument (syscall number) must be an integer type or "
                                     "a named syscall constant (e.g., READ, WRITE), got '" + argType->toString() + "'", expr);
                            return typeSystem->getErrorType();
                        }
                    }
                    // Safe tier already validated above (must be identifier in whitelist)
                    continue;
                }
                
                // Remaining args: infer type and validate
                Type* argType = inferType(expr->arguments[i].get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
                
                // Accept integer types, string, or pointer types
                PrimitiveType* prim = dynamic_cast<PrimitiveType*>(argType);
                if (prim) {
                    std::string name = prim->getName();
                    if (name == "int8" || name == "int16" || name == "int32" || name == "int64" ||
                        name == "uint8" || name == "uint16" || name == "uint32" || name == "uint64" ||
                        name == "string") {
                        continue;
                    }
                }
                PointerType* ptr = dynamic_cast<PointerType*>(argType);
                if (ptr) {
                    continue;
                }
                addError(idExpr->name + "() argument " + std::to_string(i + 1) +
                         " must be integer, string, or pointer type, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            // Return type depends on tier
            if (isRaw) {
                // sys!!!() returns bare int64 — no safety wrapping
                return typeSystem->getPrimitiveType("int64");
            } else {
                // Determine return type from syscall name (typed returns, v0.4.1)
                std::string syscallName;
                IdentifierExpr* firstArgId = dynamic_cast<IdentifierExpr*>(expr->arguments[0].get());
                if (firstArgId) {
                    syscallName = firstArgId->name;
                }
                
                auto retIt = syscallReturnTypes.find(syscallName);
                if (retIt != syscallReturnTypes.end() && retIt->second == SysReturnCategory::STRING_BUF) {
                    return typeSystem->getResultType(typeSystem->getPrimitiveType("string"));
                }
                // Default: Result<int64>
                return typeSystem->getResultType(typeSystem->getPrimitiveType("int64"));
            }
        }
        
        // ====================================================================
        // ARENA ALLOCATOR BUILTINS (Phase 4.2.5.2)
        // ====================================================================
        
        // Builtin: arena_new(int64) -> int64 (opaque handle)
        if (idExpr->name == "arena_new") {
            if (expr->arguments.size() != 1) {
                addError("arena_new() requires exactly one argument (capacity)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Returns int64 handle (0 = error)
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Builtin: arena_alloc(int64 handle, int64 size) -> int64 (pointer)
        if (idExpr->name == "arena_alloc") {
            if (expr->arguments.size() != 2) {
                addError("arena_alloc() requires two arguments (handle, size)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Returns int64 pointer (0 = error)
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Builtin: arena_reset(int64 handle) -> void
        if (idExpr->name == "arena_reset") {
            if (expr->arguments.size() != 1) {
                addError("arena_reset() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Returns void (no result) - but we'll return int32 for now
            return typeSystem->getPrimitiveType("int32");
        }
        
        // Builtin: arena_destroy(int64 handle) -> void
        if (idExpr->name == "arena_destroy") {
            if (expr->arguments.size() != 1) {
                addError("arena_destroy() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Returns void
            return typeSystem->getPrimitiveType("int32");
        }
        
        // Builtin: arena_get_allocated(int64 handle) -> int64
        if (idExpr->name == "arena_get_allocated") {
            if (expr->arguments.size() != 1) {
                addError("arena_get_allocated() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Builtin: arena_get_reserved(int64 handle) -> int64
        if (idExpr->name == "arena_get_reserved") {
            if (expr->arguments.size() != 1) {
                addError("arena_get_reserved() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // ====================================================================
        // ATOMIC<T> OPERATIONS - Lock-free Concurrency (P0)
        // ====================================================================
        
        // Builtin: atomic_new(initial_value) -> atomic<T>
        // Creates a new atomic variable with the given initial value
        // Type is inferred from argument: atomic_new(0i32) -> atomic<int32>
        if (idExpr->name == "atomic_new") {
            if (expr->arguments.size() != 1) {
                addError("atomic_new() requires exactly one argument (initial value)", expr);
                return typeSystem->getErrorType();
            }
            
            // Infer the type of the initial value
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // For now, return a generic type named "atomic_TYPENAME"
            // This will be handled specially in codegen
            std::string atomicTypeName = "atomic_" + argType->toString();
            return typeSystem->getGenericType(atomicTypeName);
        }
        
        // Pool allocator builtins
        if (idExpr->name == "pool_new") {
            if (expr->arguments.size() != 2) {
                addError("pool_new() requires exactly two arguments (block_size, initial_capacity)", expr);
                return typeSystem->getErrorType();
            }
            Type* blockSizeType = inferType(expr->arguments[0].get());
            Type* capacityType = inferType(expr->arguments[1].get());
            if (blockSizeType->getKind() == TypeKind::ERROR || capacityType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        if (idExpr->name == "pool_alloc") {
            if (expr->arguments.size() != 1) {
                addError("pool_alloc() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        if (idExpr->name == "pool_free") {
            if (expr->arguments.size() != 2) {
                addError("pool_free() requires exactly two arguments (handle, ptr)", expr);
                return typeSystem->getErrorType();
            }
            Type* handleType = inferType(expr->arguments[0].get());
            Type* ptrType = inferType(expr->arguments[1].get());
            if (handleType->getKind() == TypeKind::ERROR || ptrType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        if (idExpr->name == "pool_reset") {
            if (expr->arguments.size() != 1) {
                addError("pool_reset() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        if (idExpr->name == "pool_destroy") {
            if (expr->arguments.size() != 1) {
                addError("pool_destroy() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        if (idExpr->name == "pool_get_total_blocks") {
            if (expr->arguments.size() != 1) {
                addError("pool_get_total_blocks() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        if (idExpr->name == "pool_get_used_blocks") {
            if (expr->arguments.size() != 1) {
                addError("pool_get_used_blocks() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Slab allocator builtins
        if (idExpr->name == "slab_new") {
            if (expr->arguments.size() != 2) {
                addError("slab_new() requires exactly two arguments (object_size, slab_size)", expr);
                return typeSystem->getErrorType();
            }
            Type* objectSizeType = inferType(expr->arguments[0].get());
            Type* slabSizeType = inferType(expr->arguments[1].get());
            if (objectSizeType->getKind() == TypeKind::ERROR || slabSizeType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        if (idExpr->name == "slab_alloc") {
            if (expr->arguments.size() != 1) {
                addError("slab_alloc() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        if (idExpr->name == "slab_free") {
            if (expr->arguments.size() != 2) {
                addError("slab_free() requires exactly two arguments (handle, ptr)", expr);
                return typeSystem->getErrorType();
            }
            Type* handleType = inferType(expr->arguments[0].get());
            Type* ptrType = inferType(expr->arguments[1].get());
            if (handleType->getKind() == TypeKind::ERROR || ptrType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        if (idExpr->name == "slab_destroy") {
            if (expr->arguments.size() != 1) {
                addError("slab_destroy() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        if (idExpr->name == "slab_get_total_objects") {
            if (expr->arguments.size() != 1) {
                addError("slab_get_total_objects() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        if (idExpr->name == "slab_get_allocated_objects") {
            if (expr->arguments.size() != 1) {
                addError("slab_get_allocated_objects() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // ====================================================================
        // USER STACK BUILTINS (v0.4.3 — Per-scope Implicit Typed LIFO)
        // ====================================================================
        // astack()          → int64 (hidden handle; initializes scope stack with default capacity)
        // astack(capacity)  → int64 (hidden handle; initializes scope stack with given capacity)
        // apush(value)      → int64 (pushes value with auto-detected type tag; fatal on overflow)
        // apop()            → T (returns value typed to assignment context; runtime type-checked)
        // apeek()           → T (same as apop but non-destructive)

        // Builtin: astack() or astack(capacity) -> int64 (handle stored implicitly)
        if (idExpr->name == "astack") {
            if (expr->arguments.size() > 1) {
                addError("astack() takes 0 or 1 arguments (optional capacity in slots)", expr);
                return typeSystem->getErrorType();
            }
            if (expr->arguments.size() == 1) {
                Type* argType = inferType(expr->arguments[0].get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // Builtin: apush(value: any) -> int64 (fatal on overflow, return value discarded)
        if (idExpr->name == "apush") {
            if (expr->arguments.size() != 1) {
                addError("apush() requires exactly one argument (value to push)", expr);
                return typeSystem->getErrorType();
            }
            Type* valueType = inferType(expr->arguments[0].get());
            if (valueType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // Builtin: apop() -> T (type from assignment context, runtime type-checked)
        // Returns UnknownType so it is assignable to any destination type.
        // The codegen uses the destination type to determine the expected tag
        // and converts the raw i64 to the correct LLVM type.
        if (idExpr->name == "apop") {
            if (!expr->arguments.empty()) {
                addError("apop() takes no arguments (uses implicit scope stack)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getUnknownType();
        }

        // Builtin: apeek() -> T (same as apop but non-destructive)
        if (idExpr->name == "apeek") {
            if (!expr->arguments.empty()) {
                addError("apeek() takes no arguments (uses implicit scope stack)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getUnknownType();
        }

        // Builtin: acap() -> int64 (stack capacity in bytes)
        if (idExpr->name == "acap") {
            if (!expr->arguments.empty()) {
                addError("acap() takes no arguments (uses implicit scope stack)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // Builtin: asize() -> int64 (bytes currently used on stack)
        if (idExpr->name == "asize") {
            if (!expr->arguments.empty()) {
                addError("asize() takes no arguments (uses implicit scope stack)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // Builtin: afits(value) -> bool (true if value would fit without overflow)
        if (idExpr->name == "afits") {
            if (expr->arguments.size() != 1) {
                addError("afits() requires exactly one argument (value to check)", expr);
                return typeSystem->getErrorType();
            }
            Type* valueType = inferType(expr->arguments[0].get());
            if (valueType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // Builtin: atype() -> int64 (type tag of top stack item, -1 if empty or fast mode)
        if (idExpr->name == "atype") {
            if (!expr->arguments.empty()) {
                addError("atype() takes no arguments (uses implicit scope stack)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // ====================================================================
        // USER HASH TABLE BUILTINS (v0.4.5 — Explicit-Handle String-Keyed Hash)
        // ====================================================================
        // ahash(capacity)        → int64  (creates table, returns handle)
        // ahset(h, key, value)   → int32  (upsert, returns 0 or -1)
        // ahget(h, key)          → T      (typed retrieve, context-dependent)
        // ahcount(h)             → int64  (entry count)
        // ahsize(h)              → int64  (bytes used by stored values)
        // ahfits(h, size)        → bool   (capacity check)
        // ahtype(h, key)         → int64  (type tag of value, -1 if missing)
        // ahdelete(h, key)       → int32  (delete key, 0=ok, -1=not found)
        // ahhas(h, key)          → bool   (key existence check)
        // ahclear(h)             → void   (clear all entries)
        // ahkeys(h)              → int64  (pointer to char** array of keys)

        // Builtin: ahash(capacity_bytes) -> int64 (handle)
        if (idExpr->name == "ahash") {
            if (expr->arguments.size() != 1) {
                addError("ahash() requires exactly one argument (capacity_bytes)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // Builtin: ahset(handle, key, value) -> int32 (0=success, -1=overflow)
        if (idExpr->name == "ahset") {
            if (expr->arguments.size() != 3) {
                addError("ahset() requires exactly 3 arguments (handle, key, value)", expr);
                return typeSystem->getErrorType();
            }
            for (auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            return typeSystem->getPrimitiveType("int32");
        }

        // Builtin: ahget(handle, key) -> T (type from assignment context)
        if (idExpr->name == "ahget") {
            if (expr->arguments.size() != 2) {
                addError("ahget() requires exactly 2 arguments (handle, key)", expr);
                return typeSystem->getErrorType();
            }
            for (auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            return typeSystem->getUnknownType();
        }

        // Builtin: ahcount(handle) -> int64 (entry count)
        if (idExpr->name == "ahcount") {
            if (expr->arguments.size() != 1) {
                addError("ahcount() requires exactly 1 argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // Builtin: ahsize(handle) -> int64 (bytes used)
        if (idExpr->name == "ahsize") {
            if (expr->arguments.size() != 1) {
                addError("ahsize() requires exactly 1 argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // Builtin: ahfits(handle, value_size) -> bool
        if (idExpr->name == "ahfits") {
            if (expr->arguments.size() != 2) {
                addError("ahfits() requires exactly 2 arguments (handle, value_size)", expr);
                return typeSystem->getErrorType();
            }
            for (auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // Builtin: ahtype(handle, key) -> int64 (type tag, -1 if missing)
        if (idExpr->name == "ahtype") {
            if (expr->arguments.size() != 2) {
                addError("ahtype() requires exactly 2 arguments (handle, key)", expr);
                return typeSystem->getErrorType();
            }
            for (auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // Builtin: ahdelete(handle, key) -> int32 (0=success, -1=not found)
        if (idExpr->name == "ahdelete") {
            if (expr->arguments.size() != 2) {
                addError("ahdelete() requires exactly 2 arguments (handle, key)", expr);
                return typeSystem->getErrorType();
            }
            for (auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            return typeSystem->getPrimitiveType("int32");
        }

        // Builtin: ahhas(handle, key) -> bool (1=exists, 0=not)
        if (idExpr->name == "ahhas") {
            if (expr->arguments.size() != 2) {
                addError("ahhas() requires exactly 2 arguments (handle, key)", expr);
                return typeSystem->getErrorType();
            }
            for (auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // Builtin: ahclear(handle) -> void
        if (idExpr->name == "ahclear") {
            if (expr->arguments.size() != 1) {
                addError("ahclear() requires exactly 1 argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }

        // Builtin: ahkeys(handle) -> int64 (pointer to char** array)
        if (idExpr->name == "ahkeys") {
            if (expr->arguments.size() != 1) {
                addError("ahkeys() requires exactly 1 argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // ====================================================================
        // WILD MEMORY BUILTINS (Phase 2.2 - Manual Memory Management)
        // ====================================================================
        // These are the primitive wild memory operations that the borrow checker
        // tracks via the ALLOCATED/FREED/MAY_FREED state machine.
        //
        // Usage:
        //   wild int8@:ptr = alloc(1024);  // Allocate 1024 bytes
        //   defer free(ptr);                // Must be freed before scope exit
        //
        // For typed allocation, use sizeof:
        //   wild int64@:array = alloc(sizeof<int64>() * count);

        // Builtin: alloc(size: int64) -> wild int8@
        // Allocates 'size' bytes of wild (manual) memory
        if (idExpr->name == "alloc") {
            if (expr->arguments.size() != 1) {
                addError("alloc() requires exactly one argument (size in bytes)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Return wild int8@ (generic byte pointer)
            // The borrow checker will track this as ALLOCATED state
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));
        }

        // Builtin: free(ptr: wild any@) -> void
        // Frees a wild pointer allocated by alloc()
        if (idExpr->name == "free") {
            if (expr->arguments.size() != 1) {
                addError("free() requires exactly one argument (pointer to free)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Check that argument is a pointer type
            if (argType->getKind() != TypeKind::POINTER) {
                addError("free() requires a pointer argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            // The borrow checker will transition this to FREED state
            return typeSystem->getPrimitiveType("void");
        }

        // Builtin: realloc(ptr: wild any@, new_size: int64) -> wild int8@
        // Reallocates a wild pointer to a new size
        if (idExpr->name == "realloc") {
            if (expr->arguments.size() != 2) {
                addError("realloc() requires exactly two arguments (ptr, new_size)", expr);
                return typeSystem->getErrorType();
            }
            Type* ptrType = inferType(expr->arguments[0].get());
            Type* sizeType = inferType(expr->arguments[1].get());
            if (ptrType->getKind() == TypeKind::ERROR || sizeType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (ptrType->getKind() != TypeKind::POINTER) {
                addError("realloc() first argument must be a pointer, got '" + ptrType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));
        }

        // ====================================================================
        // STRING LIBRARY BUILTINS (Phase 4.3)
        // ====================================================================
        
        // String creation: string_from_cstr(char*) -> string
        if (idExpr->name == "string_from_cstr") {
            if (expr->arguments.size() != 1) {
                addError("string_from_cstr() requires exactly one argument (cstr)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }
        
        // String creation: string_from_char(byte) -> string
        if (idExpr->name == "string_from_char") {
            if (expr->arguments.size() != 1) {
                addError("string_from_char() requires exactly one argument (ch)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }
        
        // String basic: string_length(string) -> int64
        if (idExpr->name == "string_length") {
            if (expr->arguments.size() != 1) {
                addError("string_length() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // String basic: string_is_empty(string) -> bool
        if (idExpr->name == "string_is_empty") {
            if (expr->arguments.size() != 1) {
                addError("string_is_empty() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // String comparison: string_equals(string, string) -> bool
        if (idExpr->name == "string_equals") {
            if (expr->arguments.size() != 2) {
                addError("string_equals() requires exactly two arguments (a, b)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // String operations: string_concat(string, string) -> string
        if (idExpr->name == "string_concat") {
            if (expr->arguments.size() != 2) {
                addError("string_concat() requires exactly two arguments (a, b)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }
        
        // String operations: string_substring(string, int64, int64) -> string
        if (idExpr->name == "string_substring") {
            if (expr->arguments.size() != 3) {
                addError("string_substring() requires exactly three arguments (str, start, end)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            Type* arg3Type = inferType(expr->arguments[2].get());
            if (arg1Type->getKind() == TypeKind::ERROR || 
                arg2Type->getKind() == TypeKind::ERROR || 
                arg3Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }
        
        // String search: string_contains(string, string) -> bool
        if (idExpr->name == "string_contains") {
            if (expr->arguments.size() != 2) {
                addError("string_contains() requires exactly two arguments (haystack, needle)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // String search: string_starts_with(string, string) -> bool
        if (idExpr->name == "string_starts_with") {
            if (expr->arguments.size() != 2) {
                addError("string_starts_with() requires exactly two arguments (str, prefix)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // String search: string_ends_with(string, string) -> bool
        if (idExpr->name == "string_ends_with") {
            if (expr->arguments.size() != 2) {
                addError("string_ends_with() requires exactly two arguments (str, suffix)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // String manipulation: string_trim(string) -> string
        if (idExpr->name == "string_trim") {
            if (expr->arguments.size() != 1) {
                addError("string_trim() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }
        
        // String manipulation: string_to_upper(string) -> string
        if (idExpr->name == "string_to_upper") {
            if (expr->arguments.size() != 1) {
                addError("string_to_upper() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }
        
        // String manipulation: string_to_lower(string) -> string
        if (idExpr->name == "string_to_lower") {
            if (expr->arguments.size() != 1) {
                addError("string_to_lower() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String conversion: string_from_int(int64) -> string
        if (idExpr->name == "string_from_int") {
            if (expr->arguments.size() != 1) {
                addError("string_from_int() requires exactly one argument (value)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String conversion: string_to_int(string) -> int64
        if (idExpr->name == "string_to_int") {
            if (expr->arguments.size() != 1) {
                addError("string_to_int() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // String conversion: string_to_hex(string) -> string
        if (idExpr->name == "string_to_hex") {
            if (expr->arguments.size() != 1) {
                addError("string_to_hex() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String manipulation: string_pad_right(string, int64, int8) -> string
        if (idExpr->name == "string_pad_right") {
            if (expr->arguments.size() != 3) {
                addError("string_pad_right() requires exactly three arguments (str, total_length, pad_char)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            Type* arg3Type = inferType(expr->arguments[2].get());
            if (arg1Type->getKind() == TypeKind::ERROR ||
                arg2Type->getKind() == TypeKind::ERROR ||
                arg3Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String manipulation: string_pad_left(string, int64, int8) -> string
        if (idExpr->name == "string_pad_left") {
            if (expr->arguments.size() != 3) {
                addError("string_pad_left() requires exactly three arguments (str, total_length, pad_char)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            Type* arg3Type = inferType(expr->arguments[2].get());
            if (arg1Type->getKind() == TypeKind::ERROR ||
                arg2Type->getKind() == TypeKind::ERROR ||
                arg3Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String manipulation: string_repeat(string, int64) -> string
        if (idExpr->name == "string_repeat") {
            if (expr->arguments.size() != 2) {
                addError("string_repeat() requires exactly two arguments (str, count)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String manipulation: string_trim_start(string) -> string
        if (idExpr->name == "string_trim_start") {
            if (expr->arguments.size() != 1) {
                addError("string_trim_start() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String manipulation: string_trim_end(string) -> string
        if (idExpr->name == "string_trim_end") {
            if (expr->arguments.size() != 1) {
                addError("string_trim_end() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String search: string_index_of(string, string) -> int64  (-1 if not found)
        if (idExpr->name == "string_index_of") {
            if (expr->arguments.size() != 2) {
                addError("string_index_of() requires exactly two arguments (haystack, needle)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // Integer hex: string_from_int_hex(int64) -> string
        if (idExpr->name == "string_from_int_hex") {
            if (expr->arguments.size() != 1) {
                addError("string_from_int_hex() requires exactly one argument (value)", expr);
                return typeSystem->getErrorType();
            }
            inferType(expr->arguments[0].get());
            return typeSystem->getPrimitiveType("string");
        }

        // String conversion: string_format_float(float64, int32) -> string
        if (idExpr->name == "string_format_float") {
            if (expr->arguments.size() != 2) {
                addError("string_format_float() requires exactly two arguments (value, precision)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // ====================================================================
        // FILE I/O BUILTINS
        // ====================================================================

        // aria_write_file_simple(path: int8*, content: int8*) -> int64
        if (idExpr->name == "aria_write_file_simple") {
            if (expr->arguments.size() != 2) {
                addError("aria_write_file_simple() requires exactly two arguments (path, content)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // aria_file_exists(path: int8*) -> bool
        if (idExpr->name == "aria_file_exists") {
            if (expr->arguments.size() != 1) {
                addError("aria_file_exists() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // aria_delete_file_simple(path: int8*) -> int64
        if (idExpr->name == "aria_delete_file_simple") {
            if (expr->arguments.size() != 1) {
                addError("aria_delete_file_simple() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // aria_read_file_simple(path: int8*) -> string
        if (idExpr->name == "aria_read_file_simple") {
            if (expr->arguments.size() != 1) {
                addError("aria_read_file_simple() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // ====================================================================
        // TBB (Tagged Balanced Base) TYPE BUILTINS
        // ====================================================================

        // tbb64_from_int(int64) -> tbb64
        if (idExpr->name == "tbb64_from_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // ARIA-023: Compile-time validation of TBB sentinel values
            // tbb64 valid range: [INT64_MIN+1, INT64_MAX], sentinel: INT64_MIN
            ASTNode* arg = expr->arguments[0].get();
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    constexpr int64_t TBB64_SENTINEL = INT64_MIN;
                    if (val == TBB64_SENTINEL) {
                        addError("tbb64_from_int() cannot construct sentinel value (TBB64_ERR). Use tbb64_div(x, 0) to generate error state.", expr);
                        return typeSystem->getErrorType();
                    }
                }
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb64_is_err(tbb64) -> bool
        if (idExpr->name == "tbb64_is_err") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // tbb64_to_int(tbb64) -> int64
        if (idExpr->name == "tbb64_to_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_to_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // tbb32_from_int(int32) -> tbb32
        if (idExpr->name == "tbb32_from_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb32_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // ARIA-023: Compile-time validation of TBB sentinel values
            // tbb32 valid range: [-2147483647, 2147483647], sentinel: -2147483648
            ASTNode* arg = expr->arguments[0].get();
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    constexpr int64_t TBB32_SENTINEL = -2147483648LL;
                    constexpr int64_t TBB32_MAX = 2147483647LL;
                    if (val == TBB32_SENTINEL) {
                        addError("tbb32_from_int() cannot construct sentinel value -2147483648 (TBB32_ERR). Use tbb32_div(x, 0) to generate error state.", expr);
                        return typeSystem->getErrorType();
                    }
                    if (val < TBB32_SENTINEL || val > TBB32_MAX) {
                        addError("tbb32_from_int() value " + std::to_string(val) + " out of valid range [-2147483647, 2147483647]", expr);
                        return typeSystem->getErrorType();
                    }
                }
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb32_is_err(tbb32) -> bool
        if (idExpr->name == "tbb32_is_err") {
            if (expr->arguments.size() != 1) {
                addError("tbb32_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // tbb32_to_int(tbb32) -> int32
        if (idExpr->name == "tbb32_to_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb32_to_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }

        // tbb16_from_int(int16) -> tbb16
        if (idExpr->name == "tbb16_from_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb16_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // ARIA-023: Compile-time validation of TBB sentinel values
            // tbb16 valid range: [-32767, 32767], sentinel: -32768
            ASTNode* arg = expr->arguments[0].get();
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    constexpr int64_t TBB16_SENTINEL = -32768;
                    constexpr int64_t TBB16_MAX = 32767;
                    if (val == TBB16_SENTINEL) {
                        addError("tbb16_from_int() cannot construct sentinel value -32768 (TBB16_ERR). Use tbb16_div(x, 0) to generate error state.", expr);
                        return typeSystem->getErrorType();
                    }
                    if (val < TBB16_SENTINEL || val > TBB16_MAX) {
                        addError("tbb16_from_int() value " + std::to_string(val) + " out of valid range [-32767, 32767]", expr);
                        return typeSystem->getErrorType();
                    }
                }
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb16_is_err(tbb16) -> bool
        if (idExpr->name == "tbb16_is_err") {
            if (expr->arguments.size() != 1) {
                addError("tbb16_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // tbb16_to_int(tbb16) -> int16
        if (idExpr->name == "tbb16_to_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb16_to_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int16");
        }

        // tbb8_from_int(int8) -> tbb8
        if (idExpr->name == "tbb8_from_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb8_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // ARIA-023: Compile-time validation of TBB sentinel values
            // tbb8 valid range: [-127, 127], sentinel: -128
            ASTNode* arg = expr->arguments[0].get();
            std::optional<int64_t> constVal;
            // Check for direct literal
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    constVal = std::get<int64_t>(lit->value);
                }
            }
            // Check for unary minus with literal (e.g., -128)
            else if (arg->type == ASTNode::NodeType::UNARY_OP) {
                UnaryExpr* unary = static_cast<UnaryExpr*>(arg);
                if (unary->op.type == frontend::TokenType::TOKEN_MINUS && unary->operand) {
                    if (unary->operand->type == ASTNode::NodeType::LITERAL) {
                        LiteralExpr* lit = static_cast<LiteralExpr*>(unary->operand.get());
                        if (std::holds_alternative<int64_t>(lit->value)) {
                            constVal = -std::get<int64_t>(lit->value);
                        }
                    }
                }
            }
            if (constVal.has_value()) {
                int64_t val = constVal.value();
                if (val == -128) {
                    addError("tbb8_from_int() cannot construct sentinel value -128 (TBB8_ERR). Use tbb8_div(x, 0) to generate error state.", expr);
                    return typeSystem->getErrorType();
                }
                if (val < -127 || val > 127) {
                    addError("tbb8_from_int() value " + std::to_string(val) + " out of valid range [-127, 127]", expr);
                    return typeSystem->getErrorType();
                }
            }
            return typeSystem->getPrimitiveType("tbb8");
        }

        // tbb8_is_err(tbb8) -> bool
        if (idExpr->name == "tbb8_is_err") {
            if (expr->arguments.size() != 1) {
                addError("tbb8_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // tbb8_to_int(tbb8) -> int8
        if (idExpr->name == "tbb8_to_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb8_to_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int8");
        }

        // ====================================================================
        // TBB WIDENING INTRINSICS (ARIA-018: Sentinel-Preserving Widening)
        // ====================================================================
        // Standard sign extension would heal ERR sentinel:
        //   tbb8(-128) → sext → tbb16(-128) [VALID in tbb16, not ERR!]
        // These intrinsics preserve the ERR state across widening operations
        
        // tbb16_from_tbb8(tbb8) -> tbb16
        if (idExpr->name == "tbb16_from_tbb8") {
            if (expr->arguments.size() != 1) {
                addError("tbb16_from_tbb8() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "tbb8") {
                addError("tbb16_from_tbb8() requires tbb8 argument, got " + argType->toString(), expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }
        
        // tbb32_from_tbb8(tbb8) -> tbb32
        if (idExpr->name == "tbb32_from_tbb8") {
            if (expr->arguments.size() != 1) {
                addError("tbb32_from_tbb8() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "tbb8") {
                addError("tbb32_from_tbb8() requires tbb8 argument, got " + argType->toString(), expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }
        
        // tbb32_from_tbb16(tbb16) -> tbb32
        if (idExpr->name == "tbb32_from_tbb16") {
            if (expr->arguments.size() != 1) {
                addError("tbb32_from_tbb16() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "tbb16") {
                addError("tbb32_from_tbb16() requires tbb16 argument, got " + argType->toString(), expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }
        
        // tbb64_from_tbb8(tbb8) -> tbb64
        if (idExpr->name == "tbb64_from_tbb8") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_from_tbb8() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "tbb8") {
                addError("tbb64_from_tbb8() requires tbb8 argument, got " + argType->toString(), expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }
        
        // tbb64_from_tbb16(tbb16) -> tbb64
        if (idExpr->name == "tbb64_from_tbb16") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_from_tbb16() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "tbb16") {
                addError("tbb64_from_tbb16() requires tbb16 argument, got " + argType->toString(), expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }
        
        // tbb64_from_tbb32(tbb32) -> tbb64
        if (idExpr->name == "tbb64_from_tbb32") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_from_tbb32() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "tbb32") {
                addError("tbb64_from_tbb32() requires tbb32 argument, got " + argType->toString(), expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // ====================================================================
        // NUMERIC TYPE CONSTRUCTORS (Session 2: frac, tfp, vec9, tmatrix, ttensor)
        // ====================================================================
        
        // Fraction constructors: make_frac8, make_frac16, make_frac32, make_frac64
        if (idExpr->name == "make_frac8" || idExpr->name == "make_frac16" ||
            idExpr->name == "make_frac32" || idExpr->name == "make_frac64") {
            if (expr->arguments.size() != 3) {
                addError(idExpr->name + "() requires exactly 3 arguments (whole, numerator, denominator)", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            
            // Return the appropriate frac type
            std::string typeName = idExpr->name.substr(5);  // Remove "make_" prefix
            return typeSystem->getPrimitiveType(typeName);
        }
        
        // Tri-state floating point constructors: make_tfp32, make_tfp64
        if (idExpr->name == "make_tfp32" || idExpr->name == "make_tfp64") {
            if (expr->arguments.size() != 2) {
                addError(idExpr->name + "() requires exactly 2 arguments (exponent, mantissa)", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            
            // Return the appropriate tfp type
            std::string typeName = idExpr->name.substr(5);  // Remove "make_" prefix
            return typeSystem->getPrimitiveType(typeName);
        }
        
        // Vector constructor: make_vec9
        if (idExpr->name == "make_vec9") {
            if (expr->arguments.size() != 9) {
                addError("make_vec9() requires exactly 9 arguments (9 int32 elements)", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            
            // Return vec9 (TBB-inspired vector struct, not dvec9 float vector)
            return typeSystem->getPrimitiveType("vec9");
        }
        
        // vec9 operations
        if (idExpr->name == "vec9_add" || idExpr->name == "vec9_sub") {
            if (expr->arguments.size() != 2) {
                addError(idExpr->name + "() requires exactly 2 vec9 arguments", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments - both must be vec9
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
                // Check if argument is vec9
                if (PrimitiveType* primType = dynamic_cast<PrimitiveType*>(argType)) {
                    if (primType->getName() != "vec9") {
                        addError(idExpr->name + "() requires vec9 arguments", expr);
                        return typeSystem->getErrorType();
                    }
                } else {
                    addError(idExpr->name + "() requires vec9 arguments", expr);
                    return typeSystem->getErrorType();
                }
            }
            
            // Return vec9
            return typeSystem->getPrimitiveType("vec9");
        }
        
        if (idExpr->name == "vec9_scale") {
            if (expr->arguments.size() != 2) {
                addError("vec9_scale() requires exactly 2 arguments (vec9, int32)", expr);
                return typeSystem->getErrorType();
            }
            
            // Check first argument is vec9
            Type* arg1Type = inferType(expr->arguments[0].get());
            if (arg1Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (PrimitiveType* primType = dynamic_cast<PrimitiveType*>(arg1Type)) {
                if (primType->getName() != "vec9") {
                    addError("vec9_scale() first argument must be vec9", expr);
                    return typeSystem->getErrorType();
                }
            } else {
                addError("vec9_scale() first argument must be vec9", expr);
                return typeSystem->getErrorType();
            }
            
            // Check second argument is int32
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Return vec9
            return typeSystem->getPrimitiveType("vec9");
        }
        
        if (idExpr->name == "vec9_dot") {
            if (expr->arguments.size() != 2) {
                addError("vec9_dot() requires exactly 2 vec9 arguments", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments - both must be vec9
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
                // Check if argument is vec9
                if (PrimitiveType* primType = dynamic_cast<PrimitiveType*>(argType)) {
                    if (primType->getName() != "vec9") {
                        addError("vec9_dot() requires vec9 arguments", expr);
                        return typeSystem->getErrorType();
                    }
                } else {
                    addError("vec9_dot() requires vec9 arguments", expr);
                    return typeSystem->getErrorType();
                }
            }
            
            // Return int32 (dot product is scalar)
            return typeSystem->getPrimitiveType("int32");
        }
        
        // Tensor matrix constructor: make_tmatrix
        if (idExpr->name == "make_tmatrix") {
            if (expr->arguments.size() != 4) {
                addError("make_tmatrix() requires exactly 4 arguments (rows, cols, rank, data)", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            
            return typeSystem->getPrimitiveType("tmatrix");
        }
        
        // Tensor constructor: make_ttensor
        if (idExpr->name == "make_ttensor") {
            if (expr->arguments.size() != 11) {
                addError("make_ttensor() requires exactly 11 arguments (9 shape dims + rank + data)", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            
            return typeSystem->getPrimitiveType("ttensor");
        }
        
        // tmatrix_identity(rows, cols) -> tmatrix
        // Creates an identity matrix with specified dimensions
        if (idExpr->name == "tmatrix_identity") {
            if (expr->arguments.size() != 2) {
                addError("tmatrix_identity() requires exactly 2 arguments (rows, cols)", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments - should be integers
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            
            return typeSystem->getPrimitiveType("tmatrix");
        }
        
        // ttensor_zeros(shape) -> ttensor
        // Creates a zeroed 9D tensor with specified shape
        if (idExpr->name == "ttensor_zeros") {
            if (expr->arguments.size() != 1) {
                addError("ttensor_zeros() requires exactly 1 argument (9D shape vector or array)", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check argument
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            return typeSystem->getPrimitiveType("ttensor");
        }

        // ====================================================================
        // MAP/DICTIONARY RUNTIME FUNCTIONS
        // ====================================================================
        
        // map_new(key_size, value_size) -> wild void->
        if (idExpr->name == "map_new") {
            if (expr->arguments.size() != 2) {
                addError("map_new() requires exactly 2 arguments (key_size, value_size)", expr);
                return typeSystem->getErrorType();
            }
            
            // Return wild void-> (opaque map handle)
            Type* voidType = typeSystem->getPrimitiveType("void");
            return typeSystem->getPointerType(voidType);
        }
        
        // map_insert(map, key_ptr, value_ptr) -> void
        if (idExpr->name == "map_insert") {
            if (expr->arguments.size() != 3) {
                addError("map_insert() requires exactly 3 arguments (map, key_ptr, value_ptr)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }
        
        // map_get(map, key_ptr) -> wild void->
        if (idExpr->name == "map_get") {
            if (expr->arguments.size() != 2) {
                addError("map_get() requires exactly 2 arguments (map, key_ptr)", expr);
                return typeSystem->getErrorType();
            }
            
            Type* voidType = typeSystem->getPrimitiveType("void");
            return typeSystem->getPointerType(voidType);
        }
        
        // map_has(map, key_ptr) -> bool
        if (idExpr->name == "map_has") {
            if (expr->arguments.size() != 2) {
                addError("map_has() requires exactly 2 arguments (map, key_ptr)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // map_remove(map, key_ptr) -> void
        if (idExpr->name == "map_remove") {
            if (expr->arguments.size() != 2) {
                addError("map_remove() requires exactly 2 arguments (map, key_ptr)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }
        
        // map_length(map) -> int64
        if (idExpr->name == "map_length") {
            if (expr->arguments.size() != 1) {
                addError("map_length() requires exactly 1 argument (map)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // ====================================================================
        // OPTIONAL/RESULT HELPER FUNCTIONS
        // ====================================================================
        
        // get_optional_value(condition) -> int64?
        if (idExpr->name == "get_optional_value") {
            if (expr->arguments.size() != 1) {
                addError("get_optional_value() requires exactly 1 argument (condition)", expr);
                return typeSystem->getErrorType();
            }
            
            Type* int64Type = typeSystem->getPrimitiveType("int64");
            return typeSystem->getOptionalType(int64Type);
        }

        // ====================================================================
        // ARIA-018: TBB SENTINEL-PRESERVING WIDENING
        // ====================================================================

        // tbb_widen_8_to_16(tbb8) -> tbb16
        if (idExpr->name == "tbb_widen_8_to_16") {
            if (expr->arguments.size() != 1) {
                addError("tbb_widen_8_to_16() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb_widen_8_to_32(tbb8) -> tbb32
        if (idExpr->name == "tbb_widen_8_to_32") {
            if (expr->arguments.size() != 1) {
                addError("tbb_widen_8_to_32() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb_widen_8_to_64(tbb8) -> tbb64
        if (idExpr->name == "tbb_widen_8_to_64") {
            if (expr->arguments.size() != 1) {
                addError("tbb_widen_8_to_64() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb_widen_16_to_32(tbb16) -> tbb32
        if (idExpr->name == "tbb_widen_16_to_32") {
            if (expr->arguments.size() != 1) {
                addError("tbb_widen_16_to_32() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb_widen_16_to_64(tbb16) -> tbb64
        if (idExpr->name == "tbb_widen_16_to_64") {
            if (expr->arguments.size() != 1) {
                addError("tbb_widen_16_to_64() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb_widen_32_to_64(tbb32) -> tbb64
        if (idExpr->name == "tbb_widen_32_to_64") {
            if (expr->arguments.size() != 1) {
                addError("tbb_widen_32_to_64() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // ====================================================================
        // TBB ARITHMETIC OPERATIONS
        // ====================================================================

        // tbb64_add(tbb64, tbb64) -> tbb64
        if (idExpr->name == "tbb64_add") {
            if (expr->arguments.size() != 2) {
                addError("tbb64_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb64_sub(tbb64, tbb64) -> tbb64
        if (idExpr->name == "tbb64_sub") {
            if (expr->arguments.size() != 2) {
                addError("tbb64_sub() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb64_mul(tbb64, tbb64) -> tbb64
        if (idExpr->name == "tbb64_mul") {
            if (expr->arguments.size() != 2) {
                addError("tbb64_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb64_div(tbb64, tbb64) -> tbb64
        if (idExpr->name == "tbb64_div") {
            if (expr->arguments.size() != 2) {
                addError("tbb64_div() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb64_neg(tbb64) -> tbb64
        if (idExpr->name == "tbb64_neg") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb64_abs(tbb64) -> tbb64
        if (idExpr->name == "tbb64_abs") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb32_add(tbb32, tbb32) -> tbb32
        if (idExpr->name == "tbb32_add") {
            if (expr->arguments.size() != 2) {
                addError("tbb32_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb32_sub(tbb32, tbb32) -> tbb32
        if (idExpr->name == "tbb32_sub") {
            if (expr->arguments.size() != 2) {
                addError("tbb32_sub() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb32_mul(tbb32, tbb32) -> tbb32
        if (idExpr->name == "tbb32_mul") {
            if (expr->arguments.size() != 2) {
                addError("tbb32_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb32_div(tbb32, tbb32) -> tbb32
        if (idExpr->name == "tbb32_div") {
            if (expr->arguments.size() != 2) {
                addError("tbb32_div() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb32_neg(tbb32) -> tbb32
        if (idExpr->name == "tbb32_neg") {
            if (expr->arguments.size() != 1) {
                addError("tbb32_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb32_abs(tbb32) -> tbb32
        if (idExpr->name == "tbb32_abs") {
            if (expr->arguments.size() != 1) {
                addError("tbb32_abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb16_add(tbb16, tbb16) -> tbb16
        if (idExpr->name == "tbb16_add") {
            if (expr->arguments.size() != 2) {
                addError("tbb16_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb16_sub(tbb16, tbb16) -> tbb16
        if (idExpr->name == "tbb16_sub") {
            if (expr->arguments.size() != 2) {
                addError("tbb16_sub() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb16_mul(tbb16, tbb16) -> tbb16
        if (idExpr->name == "tbb16_mul") {
            if (expr->arguments.size() != 2) {
                addError("tbb16_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb16_div(tbb16, tbb16) -> tbb16
        if (idExpr->name == "tbb16_div") {
            if (expr->arguments.size() != 2) {
                addError("tbb16_div() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb16_neg(tbb16) -> tbb16
        if (idExpr->name == "tbb16_neg") {
            if (expr->arguments.size() != 1) {
                addError("tbb16_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb16_abs(tbb16) -> tbb16
        if (idExpr->name == "tbb16_abs") {
            if (expr->arguments.size() != 1) {
                addError("tbb16_abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb8_add(tbb8, tbb8) -> tbb8
        if (idExpr->name == "tbb8_add") {
            if (expr->arguments.size() != 2) {
                addError("tbb8_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb8");
        }

        // tbb8_sub(tbb8, tbb8) -> tbb8
        if (idExpr->name == "tbb8_sub") {
            if (expr->arguments.size() != 2) {
                addError("tbb8_sub() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb8");
        }

        // tbb8_mul(tbb8, tbb8) -> tbb8
        if (idExpr->name == "tbb8_mul") {
            if (expr->arguments.size() != 2) {
                addError("tbb8_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb8");
        }

        // tbb8_div(tbb8, tbb8) -> tbb8
        if (idExpr->name == "tbb8_div") {
            if (expr->arguments.size() != 2) {
                addError("tbb8_div() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb8");
        }

        // tbb8_neg(tbb8) -> tbb8
        if (idExpr->name == "tbb8_neg") {
            if (expr->arguments.size() != 1) {
                addError("tbb8_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb8");
        }

        // tbb8_abs(tbb8) -> tbb8
        if (idExpr->name == "tbb8_abs") {
            if (expr->arguments.size() != 1) {
                addError("tbb8_abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb8");
        }

        // ====================================================================
        // EXOTIC TYPE CONSTRUCTORS (LBIM, Fixed Point, Fractional)
        // ====================================================================
        
        // uint1024_from_int(int64) -> uint1024
        if (idExpr->name == "uint1024_from_int") {
            if (expr->arguments.size() != 1) {
                addError("uint1024_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("uint1024");
        }
        
        // int1024_from_int(int64) -> int1024
        if (idExpr->name == "int1024_from_int") {
            if (expr->arguments.size() != 1) {
                addError("int1024_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int1024");
        }
        
        // ====================================================================
        // LBIM EXPONENTIATION — int*_pow(base, exp_int64) -> int*
        // Binary exponentiation, returns ERR on overflow
        // ====================================================================
        
        if (idExpr->name == "int128_pow" || idExpr->name == "uint128_pow") {
            if (expr->arguments.size() != 2) {
                addError(idExpr->name + "() requires exactly two arguments (base, exponent)", expr);
                return typeSystem->getErrorType();
            }
            inferType(expr->arguments[0].get());
            inferType(expr->arguments[1].get());
            return typeSystem->getPrimitiveType(idExpr->name.substr(0, idExpr->name.find("_pow")));
        }
        
        if (idExpr->name == "int256_pow" || idExpr->name == "uint256_pow") {
            if (expr->arguments.size() != 2) {
                addError(idExpr->name + "() requires exactly two arguments (base, exponent)", expr);
                return typeSystem->getErrorType();
            }
            inferType(expr->arguments[0].get());
            inferType(expr->arguments[1].get());
            return typeSystem->getPrimitiveType(idExpr->name.substr(0, idExpr->name.find("_pow")));
        }
        
        if (idExpr->name == "int512_pow" || idExpr->name == "uint512_pow") {
            if (expr->arguments.size() != 2) {
                addError(idExpr->name + "() requires exactly two arguments (base, exponent)", expr);
                return typeSystem->getErrorType();
            }
            inferType(expr->arguments[0].get());
            inferType(expr->arguments[1].get());
            return typeSystem->getPrimitiveType(idExpr->name.substr(0, idExpr->name.find("_pow")));
        }
        
        if (idExpr->name == "int1024_pow" || idExpr->name == "uint1024_pow") {
            if (expr->arguments.size() != 2) {
                addError(idExpr->name + "() requires exactly two arguments (base, exponent)", expr);
                return typeSystem->getErrorType();
            }
            inferType(expr->arguments[0].get());
            inferType(expr->arguments[1].get());
            return typeSystem->getPrimitiveType(idExpr->name.substr(0, idExpr->name.find("_pow")));
        }
        
        // ====================================================================
        // STATIC TYPE CONSTANTS (Type.CONSTANT syntax)
        // Parser transforms Type.MEMBER → Type_MEMBER() zero-arg call
        // ====================================================================
        
        // TBB ERR sentinels
        if (idExpr->name == "tbb8_ERR") {
            if (expr->arguments.size() != 0) { addError("tbb8.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("tbb8");
        }
        if (idExpr->name == "tbb16_ERR") {
            if (expr->arguments.size() != 0) { addError("tbb16.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("tbb16");
        }
        if (idExpr->name == "tbb32_ERR") {
            if (expr->arguments.size() != 0) { addError("tbb32.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("tbb32");
        }
        if (idExpr->name == "tbb64_ERR") {
            if (expr->arguments.size() != 0) { addError("tbb64.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("tbb64");
        }
        
        // Balanced type ERR sentinels
        if (idExpr->name == "trit_ERR") {
            if (expr->arguments.size() != 0) { addError("trit.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("trit");
        }
        if (idExpr->name == "nit_ERR") {
            if (expr->arguments.size() != 0) { addError("nit.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("nit");
        }
        if (idExpr->name == "tryte_ERR") {
            if (expr->arguments.size() != 0) { addError("tryte.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("tryte");
        }
        if (idExpr->name == "nyte_ERR") {
            if (expr->arguments.size() != 0) { addError("nyte.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // fix256 constants
        if (idExpr->name == "fix256_ERR") {
            if (expr->arguments.size() != 0) { addError("fix256.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("fix256");
        }
        if (idExpr->name == "fix256_MAX") {
            if (expr->arguments.size() != 0) { addError("fix256.MAX takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("fix256");
        }
        if (idExpr->name == "fix256_EPSILON") {
            if (expr->arguments.size() != 0) { addError("fix256.EPSILON takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("fix256");
        }
        
        // LBIM ERR sentinels (for overflow/error detection in multi-limb arithmetic)
        if (idExpr->name == "int256_ERR") {
            if (expr->arguments.size() != 0) { addError("int256.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("int256");
        }
        if (idExpr->name == "int512_ERR") {
            if (expr->arguments.size() != 0) { addError("int512.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("int512");
        }
        if (idExpr->name == "int1024_ERR") {
            if (expr->arguments.size() != 0) { addError("int1024.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("int1024");
        }
        if (idExpr->name == "uint256_ERR") {
            if (expr->arguments.size() != 0) { addError("uint256.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("uint256");
        }
        if (idExpr->name == "uint512_ERR") {
            if (expr->arguments.size() != 0) { addError("uint512.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("uint512");
        }
        if (idExpr->name == "uint1024_ERR") {
            if (expr->arguments.size() != 0) { addError("uint1024.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("uint1024");
        }
        
        // fix256_from_int(int64) -> fix256
        if (idExpr->name == "fix256_from_int") {
            if (expr->arguments.size() != 1) {
                addError("fix256_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("fix256");
        }
        
        // fix256_from_float(flt64) -> fix256
        if (idExpr->name == "fix256_from_float") {
            if (expr->arguments.size() != 1) {
                addError("fix256_from_float() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("fix256");
        }
        
        // fix256_to_int(fix256) -> int64
        if (idExpr->name == "fix256_to_int") {
            if (expr->arguments.size() != 1) {
                addError("fix256_to_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // fix256_to_float(fix256) -> flt64
        if (idExpr->name == "fix256_to_float") {
            if (expr->arguments.size() != 1) {
                addError("fix256_to_float() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }
        
        // trit_from_int(int64) -> trit (balanced ternary: -1, 0, 1)
        if (idExpr->name == "trit_from_int") {
            if (expr->arguments.size() != 1) {
                addError("trit_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Validate range at compile time if possible
            ASTNode* arg = expr->arguments[0].get();
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    if (val < -1 || val > 1) {
                        addError("trit_from_int() value " + std::to_string(val) + " out of range [-1, 1]", expr);
                        return typeSystem->getErrorType();
                    }
                }
            }
            return typeSystem->getPrimitiveType("trit");
        }
        
        // nit_from_int(int64) -> nit (balanced nonary: -4 to +4)
        if (idExpr->name == "nit_from_int") {
            if (expr->arguments.size() != 1) {
                addError("nit_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Validate range at compile time if possible
            ASTNode* arg = expr->arguments[0].get();
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    if (val < -4 || val > 4) {
                        addError("nit_from_int() value " + std::to_string(val) + " out of range [-4, 4]", expr);
                        return typeSystem->getErrorType();
                    }
                }
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // tryte_from_int(int64) -> tryte (10 balanced ternary digits packed in uint16)
        if (idExpr->name == "tryte_from_int") {
            if (expr->arguments.size() != 1) {
                addError("tryte_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Validate range: 3^10 = 59049, balanced: -29524 to +29524
            ASTNode* arg = expr->arguments[0].get();
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    if (val < -29524 || val > 29524) {
                        addError("tryte_from_int() value " + std::to_string(val) + " out of range [-29524, 29524]", expr);
                        return typeSystem->getErrorType();
                    }
                }
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // nyte_from_int(int64) -> nyte (5 balanced nonary digits packed in uint16)
        if (idExpr->name == "nyte_from_int") {
            if (expr->arguments.size() != 1) {
                addError("nyte_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Validate range: 9^5 = 59049, balanced: -29524 to +29524
            ASTNode* arg = expr->arguments[0].get();
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    if (val < -29524 || val > 29524) {
                        addError("nyte_from_int() value " + std::to_string(val) + " out of range [-29524, 29524]", expr);
                        return typeSystem->getErrorType();
                    }
                }
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // ====================================================================
        // TRIT/NIT LOGIC AND ARITHMETIC INTRINSICS
        // ====================================================================
        // Trit: Single balanced ternary digit (-1, 0, 1, ERR=-128)
        // Nit: Single balanced nonary digit (-4 to +4, ERR=-128)
        
        // trit_add(trit, trit) -> trit
        if (idExpr->name == "trit_add") {
            if (expr->arguments.size() != 2) {
                addError("trit_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("trit");
        }
        
        // trit_mul(trit, trit) -> trit
        if (idExpr->name == "trit_mul") {
            if (expr->arguments.size() != 2) {
                addError("trit_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("trit");
        }
        
        // trit_neg(trit) -> trit
        if (idExpr->name == "trit_neg") {
            if (expr->arguments.size() != 1) {
                addError("trit_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("trit");
        }
        
        // trit_and(trit, trit) -> trit (Kleene three-valued logic)
        if (idExpr->name == "trit_and") {
            if (expr->arguments.size() != 2) {
                addError("trit_and() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("trit");
        }
        
        // trit_or(trit, trit) -> trit (Kleene three-valued logic)
        if (idExpr->name == "trit_or") {
            if (expr->arguments.size() != 2) {
                addError("trit_or() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("trit");
        }
        
        // trit_not(trit) -> trit (Kleene three-valued logic)
        if (idExpr->name == "trit_not") {
            if (expr->arguments.size() != 1) {
                addError("trit_not() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("trit");
        }
        
        // trit_is_err(trit) -> bool
        if (idExpr->name == "trit_is_err") {
            if (expr->arguments.size() != 1) {
                addError("trit_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // nit_and(nit, nit) -> nit (Nonary AND - extended Kleene logic)
        if (idExpr->name == "nit_and") {
            if (expr->arguments.size() != 2) {
                addError("nit_and() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // nit_or(nit, nit) -> nit (Nonary OR - extended Kleene logic)
        if (idExpr->name == "nit_or") {
            if (expr->arguments.size() != 2) {
                addError("nit_or() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // nit_is_err(nit) -> bool
        if (idExpr->name == "nit_is_err") {
            if (expr->arguments.size() != 1) {
                addError("nit_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // ====================================================================
        // NIT ARITHMETIC INTRINSICS
        // Skip if user has defined their own function with this name
        // ====================================================================
        
        // nit_add(nit, nit) -> nit
        if (idExpr->name == "nit_add" && !symbolTable->resolveSymbol("nit_add")) {
            if (expr->arguments.size() != 2) {
                addError("nit_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // nit_sub(nit, nit) -> nit
        if (idExpr->name == "nit_sub" && !symbolTable->resolveSymbol("nit_sub")) {
            if (expr->arguments.size() != 2) {
                addError("nit_sub() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // nit_mul(nit, nit) -> nit
        if (idExpr->name == "nit_mul" && !symbolTable->resolveSymbol("nit_mul")) {
            if (expr->arguments.size() != 2) {
                addError("nit_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // nit_div(nit, nit) -> nit
        if (idExpr->name == "nit_div" && !symbolTable->resolveSymbol("nit_div")) {
            if (expr->arguments.size() != 2) {
                addError("nit_div() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // nit_neg(nit) -> nit
        if (idExpr->name == "nit_neg" && !symbolTable->resolveSymbol("nit_neg")) {
            if (expr->arguments.size() != 1) {
                addError("nit_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // nit_abs(nit) -> nit
        if (idExpr->name == "nit_abs" && !symbolTable->resolveSymbol("nit_abs")) {
            if (expr->arguments.size() != 1) {
                addError("nit_abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // ====================================================================
        // TRYTE ARITHMETIC INTRINSICS
        // ====================================================================
        
        // tryte_add(tryte, tryte) -> tryte
        if (idExpr->name == "tryte_add") {
            if (expr->arguments.size() != 2) {
                addError("tryte_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // tryte_sub(tryte, tryte) -> tryte
        if (idExpr->name == "tryte_sub") {
            if (expr->arguments.size() != 2) {
                addError("tryte_sub() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // tryte_mul(tryte, tryte) -> tryte
        if (idExpr->name == "tryte_mul") {
            if (expr->arguments.size() != 2) {
                addError("tryte_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // tryte_div(tryte, tryte) -> tryte
        if (idExpr->name == "tryte_div") {
            if (expr->arguments.size() != 2) {
                addError("tryte_div() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // tryte_mod(tryte, tryte) -> tryte
        if (idExpr->name == "tryte_mod") {
            if (expr->arguments.size() != 2) {
                addError("tryte_mod() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // tryte_neg(tryte) -> tryte
        if (idExpr->name == "tryte_neg") {
            if (expr->arguments.size() != 1) {
                addError("tryte_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // tryte_abs(tryte) -> tryte
        if (idExpr->name == "tryte_abs") {
            if (expr->arguments.size() != 1) {
                addError("tryte_abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // tryte_is_err(tryte) -> bool
        if (idExpr->name == "tryte_is_err") {
            if (expr->arguments.size() != 1) {
                addError("tryte_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // ====================================================================
        // NYTE ARITHMETIC INTRINSICS
        // ====================================================================
        
        // nyte_add(nyte, nyte) -> nyte
        if (idExpr->name == "nyte_add") {
            if (expr->arguments.size() != 2) {
                addError("nyte_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_sub(nyte, nyte) -> nyte
        if (idExpr->name == "nyte_sub") {
            if (expr->arguments.size() != 2) {
                addError("nyte_sub() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_mul(nyte, nyte) -> nyte
        if (idExpr->name == "nyte_mul") {
            if (expr->arguments.size() != 2) {
                addError("nyte_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_div(nyte, nyte) -> nyte
        if (idExpr->name == "nyte_div") {
            if (expr->arguments.size() != 2) {
                addError("nyte_div() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_mod(nyte, nyte) -> nyte
        if (idExpr->name == "nyte_mod") {
            if (expr->arguments.size() != 2) {
                addError("nyte_mod() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_neg(nyte) -> nyte
        if (idExpr->name == "nyte_neg") {
            if (expr->arguments.size() != 1) {
                addError("nyte_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_abs(nyte) -> nyte
        if (idExpr->name == "nyte_abs") {
            if (expr->arguments.size() != 1) {
                addError("nyte_abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_is_err(nyte) -> bool
        if (idExpr->name == "nyte_is_err") {
            if (expr->arguments.size() != 1) {
                addError("nyte_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // ====================================================================
        // TRYTE/NYTE CONVERSION INTRINSICS
        // ====================================================================
        
        // tryte_from_balanced(int32) -> tryte
        if (idExpr->name == "tryte_from_balanced") {
            if (expr->arguments.size() != 1) {
                addError("tryte_from_balanced() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // nyte_from_balanced(int32) -> nyte
        if (idExpr->name == "nyte_from_balanced") {
            if (expr->arguments.size() != 1) {
                addError("nyte_from_balanced() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // tryte_to_balanced(tryte) -> int32
        if (idExpr->name == "tryte_to_balanced") {
            if (expr->arguments.size() != 1) {
                addError("tryte_to_balanced() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        // nyte_to_balanced(nyte) -> int32
        if (idExpr->name == "nyte_to_balanced") {
            if (expr->arguments.size() != 1) {
                addError("nyte_to_balanced() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        // tryte_to_nyte(tryte) -> nyte
        if (idExpr->name == "tryte_to_nyte") {
            if (expr->arguments.size() != 1) {
                addError("tryte_to_nyte() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_to_tryte(nyte) -> tryte
        if (idExpr->name == "nyte_to_tryte") {
            if (expr->arguments.size() != 1) {
                addError("nyte_to_tryte() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // ====================================================================
        // INTEGER CONVERSION BUILTINS
        // ====================================================================
        
        // int8_from_int64(int64) -> int8
        if (idExpr->name == "int8_from_int64") {
            if (expr->arguments.size() != 1) {
                addError("int8_from_int64() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int8");
        }
        
        // int16_from_int64(int64) -> int16
        if (idExpr->name == "int16_from_int64") {
            if (expr->arguments.size() != 1) {
                addError("int16_from_int64() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int16");
        }
        
        // int32_from_int64(int64) -> int32
        if (idExpr->name == "int32_from_int64") {
            if (expr->arguments.size() != 1) {
                addError("int32_from_int64() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        // int64_from_int32(int32) -> int64
        if (idExpr->name == "int64_from_int32") {
            if (expr->arguments.size() != 1) {
                addError("int64_from_int32() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // int64_from_int8(int8) -> int64
        if (idExpr->name == "int64_from_int8") {
            if (expr->arguments.size() != 1) {
                addError("int64_from_int8() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // ====================================================================
        // MEMORY MANAGEMENT BUILTINS
        // ====================================================================
        
        // ====================================================================
        // EXPLICIT SAFETY BYPASS
        // ====================================================================

        // raw(Result<T>) -> T  — Explicitly bypass "no checky no val"
        // Purpose: Extract .value without checking .is_error first.
        //          Use when you have already validated externally, or in
        //          performance-critical paths where the check is elsewhere.
        // This is the ONLY sanctioned way to skip the is_error check.
        // Example: int8:val = raw(buffer_read(buf, idx));
        if (idExpr->name == "raw") {
            if (expr->arguments.size() != 1) {
                addError("'raw' requires exactly one argument (Result<T>)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->getKind() != TypeKind::RESULT) {
                addError("'raw' argument must be Result<T> — got '" + argType->toString() + "'.\n"
                         "  'raw' extracts .value without an is_error check.\n"
                         "  If you have a plain value already, 'raw' is not needed.",
                         expr);
                return typeSystem->getErrorType();
            }
            // Bypass the KNOWN_SUCCESS gate — return inner T directly
            ResultType* resType = static_cast<ResultType*>(argType);
            return resType->getValueType();
        }

        // drop(wild T@) -> void - Free wild pointer
        if (idExpr->name == "drop") {
            if (expr->arguments.size() != 1) {
                addError("'drop' requires exactly one argument (wild pointer)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // TODO: Verify argument is a wild pointer
            return typeSystem->getPrimitiveType("void");
        }

        // sleep_ms(int64) -> void
        // Suspends execution for given number of milliseconds
        if (idExpr->name == "sleep_ms") {
            if (expr->arguments.size() != 1) {
                addError("sleep_ms() requires exactly one argument (milliseconds)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }

        // exit(int32) -> noreturn (void for type purposes)
        // Terminates the process with the given exit code.
        // Only valid in 'main' and 'failsafe' — the two program endpoints.
        if (idExpr->name == "exit") {
            if (currentFunctionName != "main" && currentFunctionName != "failsafe") {
                addError("'exit' can only be called from 'main' or 'failsafe'. "
                         "Use 'pass'/'fail' to return from regular functions.", expr);
                return typeSystem->getErrorType();
            }
            if (expr->arguments.size() != 1) {
                addError("'exit' requires exactly one argument (exit code: int32)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            PrimitiveType* argPrim = dynamic_cast<PrimitiveType*>(argType);
            if (!argPrim || argPrim->getName() != "int32") {
                addError("'exit' requires an int32 exit code, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            // In failsafe, exit code must be > 0 (error path — exit(0) is wrong)
            if (currentFunctionName == "failsafe") {
                if (expr->arguments[0]->type == ASTNode::NodeType::LITERAL) {
                    LiteralExpr* lit = static_cast<LiteralExpr*>(expr->arguments[0].get());
                    if (std::holds_alternative<int64_t>(lit->value)) {
                        int64_t val = std::get<int64_t>(lit->value);
                        if (val <= 0) {
                            addError("'exit' in failsafe must have a positive exit code (got " +
                                     std::to_string(val) + "). failsafe is an error path.", expr);
                        }
                    }
                }
            }
            return typeSystem->getPrimitiveType("void");
        }

        // env_get(string) -> string
        // Returns the value of an environment variable, or empty string if not set.
        if (idExpr->name == "env_get") {
            if (expr->arguments.size() != 1) {
                addError("env_get() requires exactly one argument (name: string)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "string") {
                addError("env_get() requires a string argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // sort_lines(string) -> string
        // Sorts newline-separated lines lexicographically.
        if (idExpr->name == "sort_lines") {
            if (expr->arguments.size() != 1) {
                addError("sort_lines() requires exactly one argument (content: string)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "string") {
                addError("sort_lines() requires a string argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // ====================================================================
        // COLLECTION BUILTINS (Phase 6.2)
        // ====================================================================

        // array_new(element_size: int64) -> int8@
        // Creates a new empty array with given element size
        if (idExpr->name == "array_new") {
            if (expr->arguments.size() != 1) {
                addError("array_new() requires exactly one argument (element_size)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Returns a pointer to AriaArray (int8@ as generic pointer)
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));
        }

        // array_length(array: ptr) -> int64
        // Returns the number of elements in the array
        if (idExpr->name == "array_length") {
            if (expr->arguments.size() != 1) {
                addError("array_length() requires exactly one argument (array)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // array_push(array: ptr, value: ptr) -> int32
        // Appends an element to the array (returns 0 on success)
        if (idExpr->name == "array_push") {
            if (expr->arguments.size() != 2) {
                addError("array_push() requires exactly two arguments (array, value)", expr);
                return typeSystem->getErrorType();
            }
            Type* arrType = inferType(expr->arguments[0].get());
            Type* valType = inferType(expr->arguments[1].get());
            if (arrType->getKind() == TypeKind::ERROR || valType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }

        // array_get(array: ptr, index: int64) -> int8@
        // Returns pointer to element at index
        if (idExpr->name == "array_get") {
            if (expr->arguments.size() != 2) {
                addError("array_get() requires exactly two arguments (array, index)", expr);
                return typeSystem->getErrorType();
            }
            Type* arrType = inferType(expr->arguments[0].get());
            Type* idxType = inferType(expr->arguments[1].get());
            if (arrType->getKind() == TypeKind::ERROR || idxType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));
        }

        // array_set(array: ptr, index: int64, value: ptr) -> int32
        // Sets element at index (returns 0 on success)
        if (idExpr->name == "array_set") {
            if (expr->arguments.size() != 3) {
                addError("array_set() requires exactly three arguments (array, index, value)", expr);
                return typeSystem->getErrorType();
            }
            Type* arrType = inferType(expr->arguments[0].get());
            Type* idxType = inferType(expr->arguments[1].get());
            Type* valType = inferType(expr->arguments[2].get());
            if (arrType->getKind() == TypeKind::ERROR || idxType->getKind() == TypeKind::ERROR ||
                valType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }

        // array_pop(array: ptr) -> int8@
        // Removes and returns pointer to last element
        if (idExpr->name == "array_pop") {
            if (expr->arguments.size() != 1) {
                addError("array_pop() requires exactly one argument (array)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));
        }

        // ====================================================================
        // MAP BUILTINS
        // ====================================================================

        // map_new(key_size: int64, value_size: int64) -> int8@
        // Creates a new empty map with given key and value sizes
        if (idExpr->name == "map_new") {
            if (expr->arguments.size() != 2) {
                addError("map_new() requires exactly two arguments (key_size, value_size)", expr);
                return typeSystem->getErrorType();
            }
            Type* keySizeType = inferType(expr->arguments[0].get());
            Type* valSizeType = inferType(expr->arguments[1].get());
            if (keySizeType->getKind() == TypeKind::ERROR || valSizeType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Returns a pointer to AriaMap (int8@ as generic pointer)
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));
        }

        // map_length(map: ptr) -> int64
        // Returns the number of key-value pairs in the map
        if (idExpr->name == "map_length") {
            if (expr->arguments.size() != 1) {
                addError("map_length() requires exactly one argument (map)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // map_insert(map: ptr, key: ptr, value: ptr) -> void
        // Inserts or updates a key-value pair in the map
        if (idExpr->name == "map_insert") {
            if (expr->arguments.size() != 3) {
                addError("map_insert() requires exactly three arguments (map, key, value)", expr);
                return typeSystem->getErrorType();
            }
            Type* mapType = inferType(expr->arguments[0].get());
            Type* keyType = inferType(expr->arguments[1].get());
            Type* valType = inferType(expr->arguments[2].get());
            if (mapType->getKind() == TypeKind::ERROR || keyType->getKind() == TypeKind::ERROR ||
                valType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }

        // map_get(map: ptr, key: ptr) -> int8@
        // Returns pointer to value associated with key, or nullptr if not found
        if (idExpr->name == "map_get") {
            if (expr->arguments.size() != 2) {
                addError("map_get() requires exactly two arguments (map, key)", expr);
                return typeSystem->getErrorType();
            }
            Type* mapType = inferType(expr->arguments[0].get());
            Type* keyType = inferType(expr->arguments[1].get());
            if (mapType->getKind() == TypeKind::ERROR || keyType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));
        }

        // map_has(map: ptr, key: ptr) -> bool
        // Returns true if the key exists in the map
        if (idExpr->name == "map_has") {
            if (expr->arguments.size() != 2) {
                addError("map_has() requires exactly two arguments (map, key)", expr);
                return typeSystem->getErrorType();
            }
            Type* mapType = inferType(expr->arguments[0].get());
            Type* keyType = inferType(expr->arguments[1].get());
            if (mapType->getKind() == TypeKind::ERROR || keyType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // map_remove(map: ptr, key: ptr) -> void
        // Removes a key-value pair from the map
        if (idExpr->name == "map_remove") {
            if (expr->arguments.size() != 2) {
                addError("map_remove() requires exactly two arguments (map, key)", expr);
                return typeSystem->getErrorType();
            }
            Type* mapType = inferType(expr->arguments[0].get());
            Type* keyType = inferType(expr->arguments[1].get());
            if (mapType->getKind() == TypeKind::ERROR || keyType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }

        // ====================================================================
        // FILE I/O BUILTINS (Phase 4.2)
        // ====================================================================
        
        // readFile(path: string) -> result<string>
        // Returns file content or error
        if (idExpr->name == "readFile") {
            if (expr->arguments.size() != 1) {
                addError("readFile() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("string"));
        }
        
        // writeFile(path: string, content: string) -> result<void>
        // Returns success/error only
        if (idExpr->name == "writeFile") {
            if (expr->arguments.size() != 2) {
                addError("writeFile() requires exactly two arguments (path, content)", expr);
                return typeSystem->getErrorType();
            }
            Type* pathType = inferType(expr->arguments[0].get());
            Type* contentType = inferType(expr->arguments[1].get());
            if (pathType->getKind() == TypeKind::ERROR || contentType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // For void results, we use result<void> (represented as ResultType with nullptr valueType or a VoidType)
            // For now, we'll return result<int64> for compatibility (0 = success in val, err = NIL)
            return typeSystem->getResultType(typeSystem->getPrimitiveType("int64"));
        }
        
        // allocate(size: int32) -> buffer@
        // Allocates wild memory and returns pointer to buffer
        if (idExpr->name == "allocate") {
            if (expr->arguments.size() != 1) {
                addError("allocate() requires exactly one argument (size)", expr);
                return typeSystem->getErrorType();
            }
            Type* sizeType = inferType(expr->arguments[0].get());
            if (sizeType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Return buffer type (represents void* wild memory pointer)
            return typeSystem->getPrimitiveType("buffer");
        }
        
        // fileExists(path: string) -> result<bool>
        // Returns whether file exists or error
        if (idExpr->name == "fileExists") {
            if (expr->arguments.size() != 1) {
                addError("fileExists() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("bool"));
        }
        
        // fileSize(path: string) -> result<int64>
        // Returns file size in bytes or error
        if (idExpr->name == "fileSize") {
            if (expr->arguments.size() != 1) {
                addError("fileSize() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("int64"));
        }
        
        // deleteFile(path: string) -> result<void>
        // Returns success/error only
        if (idExpr->name == "deleteFile") {
            if (expr->arguments.size() != 1) {
                addError("deleteFile() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // For void results, return result<int64> for compatibility (0 = success in val, err = NIL)
            return typeSystem->getResultType(typeSystem->getPrimitiveType("int64"));
        }
        
        // ====================================================================
        // PATH OPERATION BUILTINS (Phase 4.2 - Path Utils)
        // ====================================================================
        
        // pathAbsolute(path: string) -> result<string>
        // Converts a path to absolute form or returns error
        if (idExpr->name == "pathAbsolute") {
            if (expr->arguments.size() != 1) {
                addError("pathAbsolute() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("string"));
        }
        
        // pathDirname(path: string) -> result<string>
        // Returns the directory portion of a path or error
        if (idExpr->name == "pathDirname") {
            if (expr->arguments.size() != 1) {
                addError("pathDirname() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("string"));
        }
        
        // pathBasename(path: string) -> result<string>
        // Returns the filename portion of a path or error
        if (idExpr->name == "pathBasename") {
            if (expr->arguments.size() != 1) {
                addError("pathBasename() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("string"));
        }
        
        // pathJoin(dir: string, name: string) -> result<string>
        // Joins two path components or returns error
        if (idExpr->name == "pathJoin") {
            if (expr->arguments.size() != 2) {
                addError("pathJoin() requires exactly two arguments (dir, name)", expr);
                return typeSystem->getErrorType();
            }
            Type* dirType = inferType(expr->arguments[0].get());
            Type* nameType = inferType(expr->arguments[1].get());
            if (dirType->getKind() == TypeKind::ERROR || nameType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("string"));
        }
        
        // pathIsAbsolute(path: string) -> result<bool>
        // Checks if a path is absolute or returns error
        if (idExpr->name == "pathIsAbsolute") {
            if (expr->arguments.size() != 1) {
                addError("pathIsAbsolute() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("bool"));
        }

        // ====================================================================
        // FILE UFCS BUILTINS (for File.open(), stream.read(), etc.)
        // ====================================================================

        // File_open(path: string, mode: string) -> ptr (AriaStream*)
        // Static method: File.open("path", "r")
        if (idExpr->name == "File_open") {
            if (expr->arguments.size() != 2) {
                addError("File_open() requires exactly two arguments (path, mode)", expr);
                return typeSystem->getErrorType();
            }
            Type* pathType = inferType(expr->arguments[0].get());
            Type* modeType = inferType(expr->arguments[1].get());
            if (pathType->getKind() == TypeKind::ERROR || modeType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));  // ptr
        }

        // File_read_line(stream: ptr) -> string
        // Instance method: stream.read_line()
        if (idExpr->name == "File_read_line") {
            if (expr->arguments.size() != 1) {
                addError("File_read_line() requires exactly one argument (stream)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // File_read_bytes(stream: ptr, buffer: ptr, size: int64) -> int64
        if (idExpr->name == "File_read_bytes") {
            if (expr->arguments.size() != 3) {
                addError("File_read_bytes() requires exactly three arguments (stream, buffer, size)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // File_write(stream: ptr, str: string) -> int64
        // Instance method: stream.write("text")
        if (idExpr->name == "File_write") {
            if (expr->arguments.size() != 2) {
                addError("File_write() requires exactly two arguments (stream, str)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // File_write_bytes(stream: ptr, data: ptr, size: int64) -> int64
        if (idExpr->name == "File_write_bytes") {
            if (expr->arguments.size() != 3) {
                addError("File_write_bytes() requires exactly three arguments (stream, data, size)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // File_close(stream: ptr) -> void
        // Instance method: stream.close()
        if (idExpr->name == "File_close") {
            if (expr->arguments.size() != 1) {
                addError("File_close() requires exactly one argument (stream)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }

        // File_eof(stream: ptr) -> bool
        // Instance method: stream.eof()
        if (idExpr->name == "File_eof") {
            if (expr->arguments.size() != 1) {
                addError("File_eof() requires exactly one argument (stream)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // File_flush(stream: ptr) -> int32
        if (idExpr->name == "File_flush") {
            if (expr->arguments.size() != 1) {
                addError("File_flush() requires exactly one argument (stream)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }

        // File_seek(stream: ptr, offset: int64, whence: int32) -> int32
        if (idExpr->name == "File_seek") {
            if (expr->arguments.size() != 3) {
                addError("File_seek() requires exactly three arguments (stream, offset, whence)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }

        // File_tell(stream: ptr) -> int64
        if (idExpr->name == "File_tell") {
            if (expr->arguments.size() != 1) {
                addError("File_tell() requires exactly one argument (stream)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // File_read(path: string) -> string (static, reads whole file)
        if (idExpr->name == "File_read") {
            if (expr->arguments.size() != 1) {
                addError("File_read() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // File_write_all(path: string, content: string) -> int64
        if (idExpr->name == "File_write_all") {
            if (expr->arguments.size() != 2) {
                addError("File_write_all() requires exactly two arguments (path, content)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // File_delete(path: string) -> int64
        if (idExpr->name == "File_delete") {
            if (expr->arguments.size() != 1) {
                addError("File_delete() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // File_exists(path: string) -> bool
        if (idExpr->name == "File_exists") {
            if (expr->arguments.size() != 1) {
                addError("File_exists() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // File_size(path: string) -> int64
        if (idExpr->name == "File_size") {
            if (expr->arguments.size() != 1) {
                addError("File_size() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // ====================================================================
        // MATH LIBRARY BUILTINS (Phase 4.4)
        // ====================================================================
        
        // Basic math functions
        if (idExpr->name == "abs") {
            if (expr->arguments.size() != 1) {
                addError("abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Return same type as input
            return argType;
        }
        
        if (idExpr->name == "min" || idExpr->name == "max") {
            if (expr->arguments.size() != 2) {
                addError(idExpr->name + "() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Both arguments should be same type
            return arg1Type;
        }
        
        // Rounding functions - all return flt64
        if (idExpr->name == "floor" || idExpr->name == "ceil" || 
            idExpr->name == "round" || idExpr->name == "trunc") {
            if (expr->arguments.size() != 1) {
                addError(idExpr->name + "() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }
        
        // Exponential and logarithmic functions - all take/return flt64
        if (idExpr->name == "sqrt" || idExpr->name == "exp" || 
            idExpr->name == "log" || idExpr->name == "log10" || idExpr->name == "log2") {
            if (expr->arguments.size() != 1) {
                addError(idExpr->name + "() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }
        
        if (idExpr->name == "pow") {
            if (expr->arguments.size() != 2) {
                addError("pow() requires exactly two arguments (base, exponent)", expr);
                return typeSystem->getErrorType();
            }
            Type* baseType = inferType(expr->arguments[0].get());
            Type* expType = inferType(expr->arguments[1].get());
            if (baseType->getKind() == TypeKind::ERROR || expType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }
        
        // Trigonometric functions - all take/return flt64
        if (idExpr->name == "sin" || idExpr->name == "cos" || idExpr->name == "tan" ||
            idExpr->name == "asin" || idExpr->name == "acos" || idExpr->name == "atan") {
            if (expr->arguments.size() != 1) {
                addError(idExpr->name + "() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }
        
        if (idExpr->name == "atan2") {
            if (expr->arguments.size() != 2) {
                addError("atan2() requires exactly two arguments (y, x)", expr);
                return typeSystem->getErrorType();
            }
            Type* yType = inferType(expr->arguments[0].get());
            Type* xType = inferType(expr->arguments[1].get());
            if (yType->getKind() == TypeKind::ERROR || xType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }
        
        // Absolute value and float modulo - two libm functions not covered above
        if (idExpr->name == "fabs") {
            if (expr->arguments.size() != 1) {
                addError("fabs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }

        if (idExpr->name == "fmod") {
            if (expr->arguments.size() != 2) {
                addError("fmod() requires exactly two arguments (x, y)", expr);
                return typeSystem->getErrorType();
            }
            Type* xType = inferType(expr->arguments[0].get());
            Type* yType = inferType(expr->arguments[1].get());
            if (xType->getKind() == TypeKind::ERROR || yType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }

        // Mathematical constants - no arguments, return flt64
        if (idExpr->name == "PI" || idExpr->name == "E" || idExpr->name == "TAU") {
            if (expr->arguments.size() != 0) {
                addError(idExpr->name + "() is a constant and takes no arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }
    }
    
    // Infer callee type (should be function type or callable object)
    Type* calleeType = inferType(expr->callee.get());
    
    if (calleeType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Get the function declaration (if this is a direct function call)
    FuncDeclStmt* funcDecl = nullptr;
    if (expr->callee->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* idExpr = static_cast<IdentifierExpr*>(expr->callee.get());
        Symbol* funcSymbol = symbolTable->lookupSymbol(idExpr->name);
        if (funcSymbol && funcSymbol->kind == SymbolKind::FUNCTION) {
            funcDecl = funcSymbol->getFuncDecl();
        }
    }
    
    // Check if this is a generic function call
    if (funcDecl && !funcDecl->genericParams.empty() && genericResolver && monomorphizer) {
        // Infer argument types
        std::vector<Type*> argTypes;
        for (const auto& arg : expr->arguments) {
            Type* argType = inferType(arg.get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            argTypes.push_back(argType);
        }
        
        // Check for explicit type arguments (turbofish syntax)
        TypeSubstitution substitution;
        if (!expr->explicitTypeArgs.empty()) {
            // Explicit type arguments provided: identity::<int32>(42)
            substitution = genericResolver->resolveExplicitTypeArgs(funcDecl, expr->explicitTypeArgs);
        } else {
            // Type inference: identity(42)
            substitution = genericResolver->inferTypeArgs(funcDecl, expr, argTypes);
        }
        
        // Check for errors in type resolution
        if (genericResolver->hasErrors()) {
            for (const auto& err : genericResolver->getErrors()) {
                addError(err.message, expr);
            }
            genericResolver->clearErrors();
            return typeSystem->getErrorType();
        }
        
        // Request specialization
        Specialization* spec = monomorphizer->requestSpecialization(funcDecl, substitution);
        if (!spec) {
            if (monomorphizer->hasErrors()) {
                for (const auto& err : monomorphizer->getErrors()) {
                    addError(err.message, expr);
                }
            }
            return typeSystem->getErrorType();
        }
        
        // Store the mangled name in the CallExpr for IR generation
        expr->specializedMangledName = spec->mangledName;
        
        // Transitive monomorphization: walk the specialized body to find and
        // specialize any nested generic function calls (e.g. complex_add calls
        // complex_is_err internally — both need to be specialized).
        if (spec->funcDecl) {
            analyzeSpecializedBody(spec->funcDecl, spec->substitution);
        }
        
        // Resolve the specialized function's return type
        // The specialized function has concrete types substituted
        if (spec->funcDecl && spec->funcDecl->returnType) {
            Type* returnType = resolveTypeNode(spec->funcDecl->returnType.get());
            if (returnType && returnType->getKind() != TypeKind::ERROR) {
                // Aria functions return Result<T>, but pipeline calls automatically unwrap
                // Skip Result wrapping for pipeline-generated calls
                if (!expr->isPipelineCall && returnType->getKind() != TypeKind::RESULT) {
                    return typeSystem->getResultType(returnType);
                }
                return returnType;
            }
        }
        
        // Fallback: return unknown type if resolution fails
        return typeSystem->getUnknownType();
    }
    
    // Non-generic function call or generics not available
    // Validate argument count and types
    
    // If we have a function type, validate arguments
    if (calleeType->getKind() == TypeKind::FUNCTION) {
        FunctionType* funcType = static_cast<FunctionType*>(calleeType);
        
        // Check argument count
        size_t expectedCount = funcType->getParamCount();
        size_t actualCount = expr->arguments.size();
        
        if (!funcType->isVariadicFunction() && actualCount != expectedCount) {
            addError("Function expects " + std::to_string(expectedCount) + 
                    " argument(s), but " + std::to_string(actualCount) + " provided", expr);
            return typeSystem->getErrorType();
        }
        
        if (funcType->isVariadicFunction() && actualCount < expectedCount) {
            addError("Variadic function expects at least " + std::to_string(expectedCount) + 
                    " argument(s), but " + std::to_string(actualCount) + " provided", expr);
            return typeSystem->getErrorType();
        }
        
        // Check argument types
        const auto& paramTypes = funcType->getParamTypes();
        for (size_t i = 0; i < expectedCount && i < actualCount; ++i) {
            Type* argType = inferType(expr->arguments[i].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Pipeline operators automatically unwrap Result types
            // When Result<T> is passed where T is expected, we auto-unwrap
            // v0.4.3: Emit warning — error will propagate to caller if result is ERR
            Type* effectiveArgType = argType;
            if (argType->getKind() == TypeKind::RESULT) {
                ResultType* resultType = static_cast<ResultType*>(argType);
                effectiveArgType = resultType->getValueType();
                if (!expr->isPipelineCall) {
                    addWarning("Result<" + effectiveArgType->toString() + 
                              "> implicitly unwrapped in argument position — "
                              "error will propagate to caller", expr->arguments[i].get());
                }
            }
            
            // Check if argument type is assignable to parameter type
            // P1.5: Allow automatic void* ↔ wild T-> conversions at FFI boundaries
            bool ffiPointerConversion = canConvertFFIPointer(effectiveArgType, paramTypes[i]);

            // Allow implicit array-to-pointer decay: T[N] → T@ (like C array decay)
            bool arrayToPointerCoercion = false;
            if (effectiveArgType->getKind() == TypeKind::ARRAY &&
                paramTypes[i]->getKind() == TypeKind::POINTER) {
                ArrayType* arrType = static_cast<ArrayType*>(effectiveArgType);
                PointerType* ptrType = static_cast<PointerType*>(paramTypes[i]);
                if (arrType->getElementType()->isAssignableTo(ptrType->getPointeeType())) {
                    arrayToPointerCoercion = true;
                }
            }

            // v0.2.36: Allow concrete type -> dyn Trait coercion if type implements the trait
            bool dynTraitCoercion = false;
            if (paramTypes[i]->getKind() == TypeKind::DYN_TRAIT) {
                DynTraitType* dynType = static_cast<DynTraitType*>(paramTypes[i]);
                Symbol* traitSym = symbolTable->lookupSymbol(dynType->getTraitName());
                if (traitSym && traitSym->kind == SymbolKind::TRAIT) {
                    // Check if the concrete type has an impl for this trait
                    std::string concreteTypeName = effectiveArgType->toString();
                    for (const auto* implDecl : traitSym->getImplDecls()) {
                        if (implDecl->typeName == concreteTypeName) {
                            dynTraitCoercion = true;
                            break;
                        }
                    }
                }
            }

            if (!effectiveArgType->isAssignableTo(paramTypes[i]) && !ffiPointerConversion && !arrayToPointerCoercion && !dynTraitCoercion) {
                addError("Argument " + std::to_string(i + 1) + " has type '" + 
                        argType->toString() + "', but function expects '" + 
                        paramTypes[i]->toString() + "'", expr->arguments[i].get());
                // Continue checking other arguments to report all errors
            }
        }
        
        // Return the function's return type
        // For pipeline calls, return type is already unwrapped (no Result wrapping)
        Type* returnType = funcType->getReturnType();
        
        // Pipeline operators work with unwrapped values - don't re-wrap in Result
        if (expr->isPipelineCall) {
            return returnType;
        }
        
        return returnType;
    }
    
    // If callee type is not a function, it's an error
    if (calleeType->getKind() != TypeKind::UNKNOWN) {
        addError("Cannot call non-function type '" + calleeType->toString() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    // Unknown type - return unknown (for cases where type hasn't been inferred yet)
    return typeSystem->getUnknownType();
}


} // namespace sema
} // namespace aria
