// type_checker_expr.cpp — Split from type_checker.cpp for parallel builds (v0.8.2)
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

namespace npk {
namespace sema {

using namespace tc_helpers;

// ============================================================================
// Array Index Type Inference
// ============================================================================

Type* TypeChecker::inferIndexExpr(IndexExpr* expr) {
    // Infer array type
    Type* arrayType = inferType(expr->array.get());
    
    if (arrayType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Check if this is array slicing (index is a range)
    if (expr->index->type == ASTNode::NodeType::RANGE) {
        // Array slicing: arr[start..end] returns a dynamic array of same element type
        RangeExpr* rangeExpr = static_cast<RangeExpr*>(expr->index.get());
        
        // Type check the range expression
        Type* rangeType = inferType(rangeExpr);
        if (rangeType->getKind() == TypeKind::ERROR) {
            return typeSystem->getErrorType();
        }
        
        // Accept both fixed-size arrays (T[N]) and pointer/dynamic arrays (T[])
        if (arrayType->getKind() == TypeKind::ARRAY) {
            // v0.19.0 Phase 4: slicing a fixed-size array T[N] yields T[] (dynamic)
            ArrayType* arrType = static_cast<ArrayType*>(arrayType);
            Type* elemType = const_cast<Type*>(arrType->getElementType());
            return typeSystem->getArrayType(elemType, -1);
        }
        if (arrayType->getKind() == TypeKind::POINTER) {
            // Slicing a pointer/dynamic array returns the same dynamic type
            return arrayType;
        }
        addError("Cannot slice non-array type '" + arrayType->toString() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    // Regular indexing: arr[i]
    // Infer index type
    Type* indexType = inferType(expr->index.get());
    
    if (indexType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Check that index is integer type
    PrimitiveType* indexPrim = dynamic_cast<PrimitiveType*>(indexType);
    if (!indexPrim || (indexPrim->getName().find("int") != 0 && indexPrim->getName().find("uint") != 0)) {
        addError("Array index must be integer type (int8-64, uint8-64), got '" + indexType->toString() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    // Vectors support index access: vec[i]
    if (arrayType->getKind() == TypeKind::VECTOR) {
        VectorType* vecType = static_cast<VectorType*>(arrayType);
        return vecType->getComponentType();
    }
    
    // SIMD types support element access: simd<T, N>[i] -> T
    if (arrayType->getKind() == TypeKind::SIMD) {
        SimdType* simdType = static_cast<SimdType*>(arrayType);
        return simdType->getElementType();
    }
    
    // Arrays support index access: int32[10][i] -> int32
    if (arrayType->getKind() == TypeKind::ARRAY) {
        ArrayType* arrType = static_cast<ArrayType*>(arrayType);
        return arrType->getElementType();
    }
    
    // Arrays are represented as pointer types (legacy compatibility)
    // Extract element type from pointer
    if (arrayType->getKind() == TypeKind::POINTER) {
        PointerType* ptrType = static_cast<PointerType*>(arrayType);
        return ptrType->getPointeeType();
    }
    
    // Not an array/pointer/vector type
    addError("Cannot index non-array type '" + arrayType->toString() + "'", expr);
    return typeSystem->getErrorType();
}

// ============================================================================
// Member Access Type Inference
// ============================================================================

Type* TypeChecker::inferMemberAccessExpr(MemberAccessExpr* expr) {
    // Check for GPU intrinsic calls (gpu.thread_id(), gpu.sync_threads(), etc.)
    // GPU/PTX Backend Phase 4: GPU Intrinsics
    if (expr->object->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* identExpr = static_cast<IdentifierExpr*>(expr->object.get());
        if (identExpr->name == "gpu") {
            // All GPU intrinsics return int32 (except sync_threads which is void/statement-level)
            // thread_id(), block_id(), block_dim(), grid_dim() all return int32
            return typeSystem->getPrimitiveType("int32");
        }
    }
    
    // Check if this is module member access (module.function or module.symbol)
    // This must come before type inference to handle MODULE symbols correctly
    if (expr->object->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* identExpr = static_cast<IdentifierExpr*>(expr->object.get());
        Symbol* baseSym = symbolTable->resolveSymbol(identExpr->name);
        
        // If the base is a MODULE symbol, resolve member from the module
        if (baseSym && baseSym->isModule()) {
            Module* module = baseSym->getModuleRef();
            if (!module) {
                addError("Module '" + identExpr->name + "' has invalid module reference", expr);
                return typeSystem->getErrorType();
            }
            
            // Look up member in module exports
            const auto& exports = module->getExports();
            auto it = exports.find(expr->member);
            if (it != exports.end()) {
                // Found exported symbol - return its type
                return it->second.symbol->type;
            }
            
            // Member not found in module exports
            addError("Module '" + identExpr->name + "' has no exported member '" + 
                    expr->member + "'", expr);
            return typeSystem->getErrorType();
        }
        
        // Also check for enum variant access (EnumName.VARIANT)
        std::string fullName = identExpr->name + "." + expr->member;
        Symbol* variantSym = symbolTable->resolveSymbol(fullName);
        if (variantSym) {
            // Found enum variant - return its type (should be int64)
            return variantSym->type;
        }
    }
    
    // Infer object type
    Type* objectType = inferType(expr->object.get());
    
    if (objectType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Handle vector component access (.x, .y, .z for vec2/vec3, [i] for all)
    if (objectType->getKind() == TypeKind::VECTOR) {
        VectorType* vecType = static_cast<VectorType*>(objectType);
        int dimension = vecType->getDimension();
        
        // Check valid component names
        if (expr->member == "x" && dimension >= 1) {
            return vecType->getComponentType();
        }
        if (expr->member == "y" && dimension >= 2) {
            return vecType->getComponentType();
        }
        if (expr->member == "z" && dimension >= 3) {
            return vecType->getComponentType();
        }
        
        // Invalid component for this vector dimension
        std::ostringstream oss;
        oss << "vec" << dimension << " has no component '" << expr->member << "'";
        if (dimension == 2) {
            oss << " (valid: x, y)";
        } else if (dimension == 3) {
            oss << " (valid: x, y, z)";
        } else {
            oss << " (use index access [i] for vec9)";
        }
        addError(oss.str(), expr);
        return typeSystem->getErrorType();
    }
    
    // Handle Result type member access (.is_error, .value, .error)
    // Reference: runtime/result/result.cpp, include/runtime/result.h
    // ENFORCEMENT: "No checky no val" - must check .is_error before accessing .value/.error
    if (objectType->getKind() == TypeKind::RESULT) {
        ResultType* resultType = static_cast<ResultType*>(objectType);
        std::string varName = getVariableName(expr->object.get());
        
        // .is_error -> bool
        // IMPORTANT: Accessing .is_error marks this Result as checked
        if (expr->member == "is_error") {
            if (!varName.empty()) {
                markResultChecked(varName);
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // .value -> T (the wrapped value type)
        // ENFORCEMENT: Cannot access .value without first checking .is_error
        // Phase 2: Must KNOW is_error == false, not just that we checked it
        if (expr->member == "value") {
            if (!varName.empty()) {
                ResultCheckState::State state = currentResultState.getState(varName);
                if (state != ResultCheckState::State::KNOWN_SUCCESS) {
                    addError("Cannot access .value without checking .is_error first (no checky no val).\n"
                            "  Help: Add a check: if (!" + varName + ".is_error) { ... }\n"
                            "  Or use unwrap operator: value = " + varName + " ? default_value;",
                            expr);
                    return typeSystem->getErrorType();
                }
            }
            return resultType->getValueType();
        }
        
        // .error -> error code (int32)
        // ENFORCEMENT: Cannot access .error without first checking .is_error
        // Phase 2: Can access if we checked OR know is_error == true
        // Error codes are int32, matching failsafe tbb32 convention and fail() error codes.
        // When the structured error type system is added, this will return ErrorType<T>
        // with fields like .code (int32), .message (string), .source (string).
        if (expr->member == "error") {
            if (!varName.empty()) {
                ResultCheckState::State state = currentResultState.getState(varName);
                // Can access error if we checked (for logging) or know it's an error
                if (state == ResultCheckState::State::UNCHECKED) {
                    addError("Cannot access .error without checking .is_error first (no checky no val).\n"
                            "  Help: Add a check: if (" + varName + ".is_error) { ... }",
                            expr);
                    return typeSystem->getErrorType();
                }
            }
            // Error code type: int32 (matches failsafe tbb32 convention and fail() error codes)
            return typeSystem->getPrimitiveType("int32");
        }
        
        // Invalid member
        addError("result<T> has no member '" + expr->member + 
                "' (valid: is_error, value, error)", expr);
        return typeSystem->getErrorType();
    }
    
    // For safe navigation (?.) or regular access on optional types,
    // unwrap the optional to get the inner type and proceed with member access.
    // Safe navigation returns Optional<fieldType>; regular access returns fieldType directly.
    if (objectType->getKind() == TypeKind::OPTIONAL) {
        OptionalType* optType = static_cast<OptionalType*>(objectType);
        Type* innerType = optType->getWrappedType();
        
        if (innerType->getKind() == TypeKind::STRUCT) {
            StructType* structType = static_cast<StructType*>(innerType);
            const auto& fields = structType->getFields();
            
            for (const auto& field : fields) {
                if (field.name == expr->member) {
                    // Safe navigation (?.) returns Optional<fieldType>
                    if (expr->isSafeNavigation) {
                        return typeSystem->getOptionalType(field.type);
                    }
                    // Regular access on optional — return field type directly
                    return field.type;
                }
            }
            
            // Member not found
            addError("Struct '" + structType->getName() + "' has no member named '" +
                    expr->member + "'", expr);
            return typeSystem->getErrorType();
        }
        
        addError("Safe navigation requires optional of struct type, got '" +
                objectType->toString() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    // Handle struct member access
    if (objectType->getKind() == TypeKind::STRUCT) {
        StructType* structType = static_cast<StructType*>(objectType);
        const auto& fields = structType->getFields();
        
        // Look up member in struct fields
        for (const auto& field : fields) {
            if (field.name == expr->member) {
                // v0.20.4: Safe navigation (?.) on a bare struct value returns
                // optional<fieldType> so that callers can chain further ?. or ??
                // operators. The optional<T> type is fully in the type system now.
                if (expr->isSafeNavigation) {
                    return typeSystem->getOptionalType(field.type);
                }
                return field.type;
            }
        }

        // Member not found - provide enhanced error with suggestions
        std::string bestMatch;
        size_t bestDistance = SIZE_MAX;
        size_t maxDistance = std::max(size_t(2), expr->member.length() / 2);

        std::string availableFields;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) availableFields += ", ";
            availableFields += fields[i].name;

            // Check for similar field name
            size_t dist = levenshteinDistance(expr->member, fields[i].name);
            if (dist < bestDistance && dist <= maxDistance) {
                bestDistance = dist;
                bestMatch = fields[i].name;
            }
        }

        std::string errorMsg = "Struct '" + structType->getName() + "' has no member named '" +
                expr->member + "'";

        if (!bestMatch.empty()) {
            errorMsg += ". Did you mean '" + bestMatch + "'?";
        }

        if (!availableFields.empty()) {
            errorMsg += "\n  Available fields: " + availableFields;
        }

        addError(errorMsg, expr);
        return typeSystem->getErrorType();
    }
    
    // v0.27.8: Handle<T> is now an int64 alias (packed npk_handle_t),
    // not a struct. The legacy .index / .generation accessors are
    // retired — users go through HandleArena.deref(h) / HandleArena.free(h)
    // and the runtime owns the bit layout (see include/runtime/handle.h).
    if (objectType->getKind() == TypeKind::HANDLE) {
        addError("Handle<T> has no member named '" + expr->member +
                "'. Handle<T> is an opaque packed handle — use "
                "HandleArena.deref(h) to obtain the buffer pointer "
                "and HandleArena.free(h) to release it.", expr);
        return typeSystem->getErrorType();
    }
    
    // For pointer types with safe navigation, check if it's a pointer to struct
    if (objectType->getKind() == TypeKind::POINTER) {
        PointerType* ptrType = static_cast<PointerType*>(objectType);
        Type* pointeeType = ptrType->getPointeeType();

        if (pointeeType->getKind() == TypeKind::STRUCT) {
            StructType* structType = static_cast<StructType*>(pointeeType);
            const auto& fields = structType->getFields();

            // Look up member in struct fields
            for (const auto& field : fields) {
                if (field.name == expr->member) {
                    // v0.20.4: Safe navigation (?.) on ptr<struct> returns
                    // optional<fieldType> so the result can be chained with ??
                    if (expr->isSafeNavigation) {
                        return typeSystem->getOptionalType(field.type);
                    }
                    return field.type;
                }
            }

            // Member not found - provide enhanced error with suggestions
            std::string bestMatch;
            size_t bestDistance = SIZE_MAX;
            size_t maxDistance = std::max(size_t(2), expr->member.length() / 2);

            std::string availableFields;
            for (size_t i = 0; i < fields.size(); ++i) {
                if (i > 0) availableFields += ", ";
                availableFields += fields[i].name;

                size_t dist = levenshteinDistance(expr->member, fields[i].name);
                if (dist < bestDistance && dist <= maxDistance) {
                    bestDistance = dist;
                    bestMatch = fields[i].name;
                }
            }

            std::string errorMsg = "Struct '" + structType->getName() + "' has no member named '" +
                    expr->member + "'";

            if (!bestMatch.empty()) {
                errorMsg += ". Did you mean '" + bestMatch + "'?";
            }

            if (!availableFields.empty()) {
                errorMsg += "\n  Available fields: " + availableFields;
            }

            addError(errorMsg, expr);
            return typeSystem->getErrorType();
        }
    }
    
    // Handle dyn Trait types — member access dispatches through the vtable.
    // The actual method resolution happens in inferCallExpr via UFCS; here
    // we accept the member access and return unknown so the call path can resolve it.
    if (objectType->getKind() == TypeKind::DYN_TRAIT) {
        return typeSystem->getUnknownType();
    }

    // Handle obj (opaque object) — member access is always dynamic
    if (objectType->getKind() == TypeKind::PRIMITIVE) {
        PrimitiveType* prim = static_cast<PrimitiveType*>(objectType);
        if (prim->getName() == "obj") {
            return typeSystem->getUnknownType();
        }
    }

    addError("Member access (.) requires struct, object, or union type, got '" + 
            objectType->toString() + "'. Use -> for pointer member access.", expr);
    return typeSystem->getErrorType();
}

// ============================================================================
// Ternary Expression Type Inference
// ============================================================================

Type* TypeChecker::inferTernaryExpr(TernaryExpr* expr) {
    // Infer condition type
    Type* condType = inferType(expr->condition.get());
    
    if (condType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Condition must be bool
    PrimitiveType* condPrim = dynamic_cast<PrimitiveType*>(condType);
    if (!condPrim || condPrim->getName() != "bool") {
        addError("Ternary condition must be 'bool' type, got '" + 
                condType->toString() + "'. Use explicit comparison (e.g., x != 0) instead of truthiness.", expr->condition.get());
        return typeSystem->getErrorType();
    }
    
    // Infer branch types
    Type* trueType = inferType(expr->trueValue.get());
    Type* falseType = inferType(expr->falseValue.get());
    
    if (trueType->getKind() == TypeKind::ERROR || falseType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Find common type for branches
    Type* resultType = findCommonType(trueType, falseType);
    
    if (resultType->getKind() == TypeKind::ERROR) {
        addError("Ternary operator branches have incompatible types: '" + 
                trueType->toString() + "' and '" + falseType->toString() + "'", expr);
    }
    
    return resultType;
}

Type* TypeChecker::inferUnwrapExpr(UnwrapExpr* expr) {
    // Infer the result expression type
    Type* resultType = inferType(expr->result.get());
    
    if (resultType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Infer the default value type
    Type* defaultType = inferType(expr->defaultValue.get());
    
    if (defaultType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Three operators use UnwrapExpr:
    // ? operator: result_expr ? default (unwraps Result<T>)
    // ?? operator: nullable_expr ?? default (null coalescing for pointers and optionals)
    // ?! operator: result_expr ?! default (emphatic unwrap for Result<T>)
    
    // Determine operator name for error messages
    std::string opName = expr->isFailsafe ? "?!" : (expr->isNullCoalesce ? "??" : "?");
    
    Type* valueType = resultType;
    
    if (resultType->getKind() == TypeKind::RESULT) {
        // ? or ?! operator: Extract T from Result<T>
        ResultType* resType = static_cast<ResultType*>(resultType);
        valueType = resType->getValueType();
        
        // Fits-or-fails for unsuffixed integer literals:
        // If the default is an unsuffixed integer literal (defaults to int32),
        // check if the value fits in the Result's value type (e.g., tbb32).
        // This mirrors how variable declarations handle `tbb32:x = 42;`.
        if (!defaultType->isAssignableTo(valueType) && !canCoerce(defaultType, valueType)) {
            bool literalRetyped = false;
            if (expr->defaultValue->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(expr->defaultValue.get());
                if (lit->explicit_type.empty() && std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    if (integerFitsInType(val, valueType->toString())) {
                        defaultType = valueType;
                        literalRetyped = true;
                    }
                }
            }
            if (!literalRetyped) {
                addError("Unwrap operator (" + opName + ") default value type '" + defaultType->toString() + 
                        "' does not match result value type '" + valueType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
        }
    } 
    else if (resultType->getKind() == TypeKind::POINTER) {
        // Emphatic unwrap (?!) only works on Result<T>, not pointers
        if (expr->isFailsafe) {
            addError("Emphatic unwrap operator (?!) requires Result<T> type, got '" + 
                    resultType->toString() + "'. Use ?? for null coalescing on pointers.", expr);
            return typeSystem->getErrorType();
        }
        
        // ?? operator: Extract T from ptr T
        PointerType* ptrType = static_cast<PointerType*>(resultType);
        valueType = ptrType->getPointeeType();
        
        // Check that default value type matches the pointee type
        if (!defaultType->isAssignableTo(valueType) && !canCoerce(defaultType, valueType)) {
            addError("Null coalesce operator (??) default value type '" + defaultType->toString() + 
                    "' does not match pointer target type '" + valueType->toString() + "'", expr);
            return typeSystem->getErrorType();
        }
    }
    else if (resultType->getKind() == TypeKind::OPTIONAL) {
        // Emphatic unwrap (?!) only works on Result<T>, not optionals
        if (expr->isFailsafe) {
            addError("Emphatic unwrap operator (?!) requires Result<T> type, got '" + 
                    resultType->toString() + "'. Use ?? for null coalescing on optionals.", expr);
            return typeSystem->getErrorType();
        }
        
        // ?? operator: Extract T from Optional<T> (T?)
        OptionalType* optType = static_cast<OptionalType*>(resultType);
        valueType = optType->getWrappedType();
        
        // Check that default value type matches the wrapped type
        if (!defaultType->isAssignableTo(valueType) && !canCoerce(defaultType, valueType)) {
            addError("Null coalesce operator (??) default value type '" + defaultType->toString() + 
                    "' does not match optional type '" + valueType->toString() + "'", expr);
            return typeSystem->getErrorType();
        }
    }
    else if (resultType->getKind() == TypeKind::UNKNOWN) {
        // Unknown call result (e.g. function pointer call whose return type isn't
        // tracked by the type checker yet).  Trust the default value's type.
        return defaultType;
    }
    else {
        std::string validTypes = expr->isFailsafe ? 
            "Result<T>" : 
            "Result<T>, Optional<T>, or ptr T";
        addError("Unwrap operator (" + opName + ") requires " + validTypes + " type, got '" + 
                resultType->toString() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    // All operators return the value type (T from Result<T>, Optional<T>, or ptr T)
    return valueType;
}

Type* TypeChecker::inferDefaultsExpr(DefaultsExpr* expr) {
    // defaults / ?| — scoped expression fallback (v0.4.3)
    // The sub-expression may produce Result<T> at any point during evaluation.
    // If any intermediate ERR occurs, the whole expression short-circuits to the fallback.
    // The return type is T (the unwrapped value type of the sub-expression).
    
    Type* exprType = inferType(expr->expr.get());
    if (exprType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    Type* fallbackType = inferType(expr->fallback.get());
    if (fallbackType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Enforce: fallback must be literal, variable, or unknown (parser already validates this,
    // but double-check at the type checker level for safety)
    ASTNode* fb = expr->fallback.get();
    if (fb->type != ASTNode::NodeType::LITERAL && fb->type != ASTNode::NodeType::IDENTIFIER) {
        addError("Fallback for 'defaults'/'?|' must be a literal, variable, or 'unknown' — "
                 "not a function call or compound expression", expr);
        return typeSystem->getErrorType();
    }
    
    // Determine the effective value type — unwrap Result<T> if present
    Type* valueType = exprType;
    if (exprType->getKind() == TypeKind::RESULT) {
        ResultType* resType = static_cast<ResultType*>(exprType);
        valueType = resType->getValueType();
    }
    
    // unknown fallback is always valid — it's the sentinel pattern
    if (fallbackType->getKind() == TypeKind::UNKNOWN) {
        return valueType;
    }
    
    // Check that fallback type matches the value type (with literal retyping)
    if (!fallbackType->isAssignableTo(valueType) && !canCoerce(fallbackType, valueType)) {
        bool literalRetyped = false;
        if (fb->type == ASTNode::NodeType::LITERAL) {
            LiteralExpr* lit = static_cast<LiteralExpr*>(fb);
            if (lit->explicit_type.empty() && std::holds_alternative<int64_t>(lit->value)) {
                int64_t val = std::get<int64_t>(lit->value);
                if (integerFitsInType(val, valueType->toString())) {
                    fallbackType = valueType;
                    literalRetyped = true;
                }
            }
        }
        if (!literalRetyped) {
            addError("defaults/?| fallback type '" + fallbackType->toString() + 
                    "' does not match expression type '" + valueType->toString() + "'", expr);
            return typeSystem->getErrorType();
        }
    }
    
    return valueType;
}


Type* TypeChecker::inferMoveExpr(MoveExpr* expr) {
    // Move expression transfers ownership of a variable
    // The type is the same as the variable being moved
    Type* varType = inferType(expr->variable.get());
    
    if (varType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // For now, we just return the same type
    // The borrow checker will handle tracking the move and invalidation
    return varType;
}

Type* TypeChecker::inferObjectLiteral(ObjectLiteralExpr* expr) {
    // If type_name is set, this is a struct constructor
    if (!expr->type_name.empty()) {
        // Result{val:..., err:..., is_error:...} — explicit Result construction (ToS operator)
        // Infer type from the enclosing function's return type
        if (expr->type_name == "Result") {
            if (currentFunctionReturnType) {
                return currentFunctionReturnType;
            }
            addError("Result{...} literal used outside of a function context", expr);
            return typeSystem->getErrorType();
        }
        
        // Lookup struct type
        Type* structType = typeSystem->getStructType(expr->type_name);
        
        if (!structType) {
            addError("Unknown struct type: '" + expr->type_name + "'", expr);
            return typeSystem->getErrorType();
        }
        
        // Return the struct type
        // Field validation is done in checkVarDecl for positional literals
        // or in parsePostfix for named struct literals
        return structType;
    }
    
    // Dynamic object literal (no type_name): return obj type
    Type* objType = typeSystem->getPrimitiveType("obj");
    if (!objType) {
        addError("Internal error: 'obj' type not registered in type system. This is a compiler bug.", expr);
        return typeSystem->getErrorType();
    }
    
    return objType;
}

// ============================================================================
// Array Literal Type Inference
// ============================================================================

Type* TypeChecker::inferArrayLiteral(ArrayLiteralExpr* expr) {
    // Empty array literal: type cannot be inferred without context
    if (expr->elements.empty()) {
        addError("Cannot infer type of empty array literal []. "
                "Provide an explicit type: Type:name = []; or use a non-empty initializer.", expr);
        return typeSystem->getErrorType();
    }
    
    // Infer type from first element
    Type* elementType = inferType(expr->elements[0].get());
    if (elementType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Check all elements have compatible types
    for (size_t i = 1; i < expr->elements.size(); ++i) {
        Type* elemType = inferType(expr->elements[i].get());
        if (elemType->getKind() == TypeKind::ERROR) {
            return typeSystem->getErrorType();
        }
        
        if (!elemType->equals(elementType) && !canCoerce(elemType, elementType)) {
            addError("Array element " + std::to_string(i) + " has incompatible type: " +
                    "expected '" + elementType->toString() + "', got '" + elemType->toString() + "'", 
                    expr->elements[i].get());
            return typeSystem->getErrorType();
        }
    }
    
    // P2.7 FIX: Return proper array type with size, not pointer type
    // Array literals should have known size at compile time
    size_t arraySize = expr->elements.size();
    return typeSystem->getArrayType(elementType, arraySize);
}

// ============================================================================
// Range Expression Type Inference
// ============================================================================

Type* TypeChecker::inferRangeExpr(RangeExpr* expr) {
    // Check start expression
    Type* startType = inferType(expr->start.get());
    if (startType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Check end expression
    Type* endType = inferType(expr->end.get());
    if (endType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Verify both are integer types (ranges only work with integers)
    PrimitiveType* startPrim = dynamic_cast<PrimitiveType*>(startType);
    PrimitiveType* endPrim = dynamic_cast<PrimitiveType*>(endType);
    
    if (!startPrim || !endPrim) {
        addError("Range expressions require integer types, got '" + 
                startType->toString() + "' and '" + endType->toString() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    // Check if both are integer types
    bool startIsInt = (startPrim->getName().find("int") != std::string::npos || 
                       startPrim->getName().find("tbb") != std::string::npos ||
                       startPrim->getName().find("bal") != std::string::npos);
    bool endIsInt = (endPrim->getName().find("int") != std::string::npos ||
                     endPrim->getName().find("tbb") != std::string::npos ||
                     endPrim->getName().find("bal") != std::string::npos);
    
    if (!startIsInt || !endIsInt) {
        addError("Range expressions require integer types, got '" + 
                startType->toString() + "' and '" + endType->toString() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    // Range type is opaque - just return int64 as a placeholder
    // The actual range is a struct {int64, int64, bool} created during codegen
    return typeSystem->getPrimitiveType("int64");
}

// ============================================================================
// Type Compatibility and Coercion
// ============================================================================

Type* TypeChecker::findCommonType(Type* left, Type* right) {
    // If types are equal, return either one
    if (left->equals(right)) {
        return left;
    }
    
    // Try coercion in both directions
    if (canCoerce(left, right)) {
        return right;  // Widen left to right
    }
    
    if (canCoerce(right, left)) {
        return left;  // Widen right to left
    }
    
    // No common type found
    addError("No common type between '" + left->toString() + 
            "' and '" + right->toString() + "'", 0, 0);
    return typeSystem->getErrorType();
}

bool TypeChecker::canCoerce(Type* from, Type* to) {
    // Same type: always coercible
    if (from->equals(to)) {
        return true;
    }
    
    // any type accepts assignment from any other type (type erasure)
    // This is the entry point for type-erased storage.
    // Getting the value back out requires resolve<T>() or get<T>().
    if (to->getKind() == TypeKind::ANY) {
        return true;
    }
    
    // Only handle primitive type coercion for now
    PrimitiveType* fromPrim = dynamic_cast<PrimitiveType*>(from);
    PrimitiveType* toPrim = dynamic_cast<PrimitiveType*>(to);
    
    if (!fromPrim || !toPrim) {
        return false;  // Non-primitive types require exact match
    }
    
    const std::string& fromName = fromPrim->getName();
    const std::string& toName = toPrim->getName();
    
    // ========================================================================
    // Aria STRICT Type Coercion Rules
    // ========================================================================
    // - NO implicit integer widening (int32 → int64 requires explicit cast)
    // - NO cross-family coercion (int ↔ tbb ↔ balanced FORBIDDEN)
    // - NO implicit TBB widening (ARIA-018: Sentinel Discontinuity Constraint)
    //   tbb8 ERR (-128) → tbb16 becomes valid -128, not ERR (-32768)
    //   Use explicit tbb_widen<T>() for safe sentinel-preserving conversion
    // - Only float → float widening allowed
    // - Only balanced → same-subfamily balanced widening allowed
    // ========================================================================

    // Extract bit width from type name (e.g., "int32" → 32)
    // Returns 0 for invalid or malformed type names (overflow, non-numeric, etc.)
    auto extractBitWidth = [](const std::string& name) -> int {
        try {
            std::string suffix;
            if (name.find("int") == 0 || name.find("uint") == 0) {
                size_t pos = (name[0] == 'u') ? 4 : 3;  // "uint" or "int"
                suffix = name.substr(pos);
            } else if (name.find("flt") == 0) {
                suffix = name.substr(3);
            } else if (name.find("tbb") == 0) {
                suffix = name.substr(3);
            } else {
                return 0;
            }

            // Use stoll to handle larger values, then validate range
            long long width = std::stoll(suffix);

            // Valid bit widths for Aria types
            if (width == 8 || width == 16 || width == 32 || width == 64 ||
                width == 128 || width == 256 || width == 512 || width == 1024 || width == 2048 || width == 4096) {
                return static_cast<int>(width);
            }
            return 0;  // Invalid bit width
        } catch (const std::out_of_range&) {
            return 0;  // Overflow - not a valid bit width
        } catch (const std::invalid_argument&) {
            return 0;  // Not a valid number
        }
    };

    // Check type families
    bool fromIsTBB = (fromName.find("tbb") == 0);
    bool toIsTBB = (toName.find("tbb") == 0);
    bool fromIsBalanced = (fromName == "trit" || fromName == "tryte" ||
                          fromName == "nit" || fromName == "nyte");
    bool toIsBalanced = (toName == "trit" || toName == "tryte" ||
                        toName == "nit" || toName == "nyte");
    bool fromIsFloat = (fromName.find("flt") == 0);
    bool toIsFloat = (toName.find("flt") == 0);
    bool fromIsInt = ((fromName.find("int") == 0 || fromName.find("uint") == 0) && !fromIsTBB);
    bool toIsInt = ((toName.find("int") == 0 || toName.find("uint") == 0) && !toIsTBB);

    // ========================================================================
    // TBB Type Family - NO implicit coercion (even between TBB sizes)
    // ========================================================================
    // ARIA-018: TBB sizes have different sentinel values:
    //   tbb8 sentinel = -128, tbb16 sentinel = -32768, etc.
    // Widening -128 from tbb8 to tbb16 would create a valid value,
    // not a sentinel, breaking sticky error semantics.
    // Use explicit tbb_widen<T>() functions for size conversion.
    if (toIsTBB) {
        if (!fromIsTBB) {
            return false;  // REJECT: int → tbb, balanced → tbb, float → tbb
        }
        // REJECT: tbb8 → tbb16, etc. (different sentinel values)
        // Only allow exact same type
        return fromName == toName;
    }
    if (fromIsTBB) {
        return false;  // REJECT: tbb → anything else
    }

    // ========================================================================
    // Balanced Type Family - ONLY same-subfamily widening
    // ========================================================================
    if (toIsBalanced) {
        if (!fromIsBalanced) {
            return false;  // REJECT: int → balanced, float → balanced
        }
        // Only same sub-family widening: trit → tryte, nit → nyte
        bool fromIsTernary = (fromName == "trit" || fromName == "tryte");
        bool toIsTernary = (toName == "trit" || toName == "tryte");
        if (fromIsTernary != toIsTernary) {
            return false;  // REJECT: ternary ↔ nonary
        }
        // Allow widening within same sub-family
        // trit (2 bits) → tryte (16 bits), nit (4 bits) → nyte (16 bits)
        if (fromName == "trit" && toName == "tryte") return true;
        if (fromName == "nit" && toName == "nyte") return true;
        return false;  // No other balanced widening
    }
    if (fromIsBalanced) {
        return false;  // REJECT: balanced → int, balanced → float
    }

    // ========================================================================
    // Float Widening - ONLY float → float widening allowed
    // ========================================================================
    if (fromIsFloat && toIsFloat) {
        int fromWidth = extractBitWidth(fromName);
        int toWidth = extractBitWidth(toName);
        return fromWidth < toWidth;  // Allow widening only
    }

    // ========================================================================
    // fix256 literal initialization from int or float
    // ========================================================================
    // fix256:x = 1.0 or fix256:x = 42 auto-converts via runtime
    if (toName == "fix256" && (fromIsFloat || fromIsInt)) {
        return true;
    }

    // REJECT: int → float (requires explicit cast)
    if (fromIsInt && toIsFloat) {
        return false;
    }

    // REJECT: float → int
    if (fromIsFloat && toIsInt) {
        return false;
    }

    // ========================================================================
    // Integer Widening - ALLOWED within same family
    // ========================================================================
    // int8 → int16 → int32 → int64 OK (signed family)
    // uint8 → uint16 → uint32 → uint64 OK (unsigned family)
    // int ↔ uint REJECTED (sign change)

    bool fromIsSigned = (fromName.find("int") == 0 && fromName.find("uint") == std::string::npos);
    bool toIsSigned = (toName.find("int") == 0 && toName.find("uint") == std::string::npos);
    bool fromIsUnsigned = (fromName.find("uint") == 0);
    bool toIsUnsigned = (toName.find("uint") == 0);

    // Signed integer widening
    if (fromIsSigned && toIsSigned) {
        int fromWidth = extractBitWidth(fromName);
        int toWidth = extractBitWidth(toName);
        return fromWidth < toWidth;  // Allow widening only
    }

    // Unsigned integer widening
    if (fromIsUnsigned && toIsUnsigned) {
        int fromWidth = extractBitWidth(fromName);
        int toWidth = extractBitWidth(toName);
        return fromWidth < toWidth;  // Allow widening only
    }

    // REJECT: signed ↔ unsigned (int32 ↔ uint32)
    return false;
}

// ============================================================================
// FFI Pointer Conversion (P1.5 - void* ↔ wild T-> Bridge)
// ============================================================================

bool TypeChecker::canConvertFFIPointer(Type* from, Type* to) {
    // Check if one type is void* (C FFI) and the other is a wild pointer (Aria)
    
    std::string fromStr = from->toString();
    std::string toStr = to->toString();
    
    // void* → wild T-> conversion (from extern function return value)
    // Example: wild int32->:ptr = malloc(400);
    if (fromStr == "void*" && to->getKind() == TypeKind::POINTER) {
        return true;  // Allow automatic conversion from void* to any wild pointer
    }
    
    // wild T-> → void* conversion (to extern function parameter)
    // Example: free(ptr) where ptr is wild int32->
    if (from->getKind() == TypeKind::POINTER && toStr == "void*") {
        return true;  // Allow automatic conversion from any wild pointer to void*
    }
    
    return false;
}

// ============================================================================
// Error Handling
// ============================================================================

void TypeChecker::addError(const std::string& message, int line, int column) {
    std::ostringstream oss;
    if (line > 0) {
        oss << "Line " << line << ", Column " << column << ": ";
    }
    oss << message;
    errors.push_back(oss.str());
}

void TypeChecker::addError(const std::string& message, ASTNode* node) {
    if (node) {
        addError(message, node->line, node->column);
    } else {
        addError(message, 0, 0);
    }
}

void TypeChecker::addWarning(const std::string& message, ASTNode* node) {
    std::ostringstream oss;
    if (node && node->line > 0) {
        oss << "Line " << node->line << ", Column " << node->column << ": ";
    }
    oss << "warning: " << message;
    warnings.push_back(oss.str());
}


} // namespace sema
} // namespace npk
