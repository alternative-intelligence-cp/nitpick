// type_checker_stmts.cpp — Split from type_checker.cpp for parallel builds (v0.8.2)
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
// Statement Type Checking - Phase 3.2.3
// ============================================================================

void TypeChecker::checkStatement(ASTNode* stmt) {
    if (!stmt) {
        return;
    }
    
    switch (stmt->type) {
        case ASTNode::NodeType::VAR_DECL:
            checkVarDecl(static_cast<VarDeclStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::FUNC_DECL:
            checkFuncDecl(static_cast<FuncDeclStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::STRUCT_DECL:
            checkStructDecl(static_cast<StructDeclStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::ENUM_DECL:
            checkEnumDecl(static_cast<EnumDeclStmt*>(stmt));
            break;

        case ASTNode::NodeType::RULES_DECL:
            checkRulesDecl(static_cast<RulesDeclStmt*>(stmt));
            break;

        case ASTNode::NodeType::TYPE_DECL:
            checkTypeDecl(static_cast<TypeDeclStmt*>(stmt));
            break;

        case ASTNode::NodeType::TRAIT_DECL:
            checkTraitDecl(static_cast<TraitDeclStmt*>(stmt));
            break;

        case ASTNode::NodeType::IMPL_DECL:
            checkImplDecl(static_cast<ImplDeclStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::MACRO_DECL:
            checkMacroDecl(static_cast<MacroDeclStmt*>(stmt));
            break;

        case ASTNode::NodeType::RETURN:
            checkReturnStmt(static_cast<ReturnStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::PASS:
            checkPassStmt(static_cast<PassStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::FAIL:
            checkFailStmt(static_cast<FailStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::PROVE:
            checkProveStmt(static_cast<ProveStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::ASSERT_STATIC:
            checkAssertStaticStmt(static_cast<AssertStaticStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::IF:
            checkIfStmt(static_cast<IfStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::WHILE:
            checkWhileStmt(static_cast<WhileStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::FOR:
            checkForStmt(static_cast<ForStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::TILL:
            checkTillStmt(static_cast<TillStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::LOOP:
            checkLoopStmt(static_cast<LoopStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::WHEN:
            checkWhenStmt(static_cast<WhenStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::BLOCK:
            checkBlockStmt(static_cast<BlockStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::EXPRESSION_STMT:
            checkExpressionStmt(static_cast<ExpressionStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::BREAK:
        case ASTNode::NodeType::CONTINUE:
            // No type checking needed for break/continue
            break;
        
        case ASTNode::NodeType::USE:
            checkUseStmt(static_cast<UseStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::MOD:
            checkModStmt(static_cast<ModStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::PICK:
            checkPickStmt(static_cast<PickStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::EXTERN:
            checkExternStmt(static_cast<ExternStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::COMPTIME_BLOCK:
            checkComptimeBlock(static_cast<ComptimeBlockStmt*>(stmt));
            break;
        
        default:
            // Other statement types not yet implemented
            break;
    }
}

// ============================================================================
// Type Resolution from AST Nodes
// ============================================================================

Type* TypeChecker::resolveTypeNode(ASTNode* typeNode) {
    if (!typeNode) {
        return typeSystem->getErrorType();
    }
    
    switch (typeNode->type) {
        case ASTNode::NodeType::TYPE_ANNOTATION: {
            // Simple type: int32, string, etc.
            aria::SimpleType* simpleType = static_cast<aria::SimpleType*>(typeNode);
            
            // Handle vector types specially (vec2, vec3)
            // Default to flt64 components to match float literal inference
            if (simpleType->typeName == "vec2" || simpleType->typeName == "dvec2") {
                Type* componentType = typeSystem->getPrimitiveType("flt64");
                return typeSystem->getVectorType(componentType, 2);
            }
            if (simpleType->typeName == "vec3" || simpleType->typeName == "dvec3") {
                Type* componentType = typeSystem->getPrimitiveType("flt64");
                return typeSystem->getVectorType(componentType, 3);
            }
            // dvec9: Double-precision floating-point vector (9 flt64 components)
            if (simpleType->typeName == "dvec9") {
                Type* componentType = typeSystem->getPrimitiveType("flt64");
                return typeSystem->getVectorType(componentType, 9);
            }
            // vec9: TBB-inspired vector (vec9_tbb32 struct - NOT a VectorType)
            // Handled below via struct/primitive lookup
            
            // Float vector types (fvec2, fvec3, fvec9)
            if (simpleType->typeName == "fvec2") {
                Type* componentType = typeSystem->getPrimitiveType("flt32");
                return typeSystem->getVectorType(componentType, 2);
            }
            if (simpleType->typeName == "fvec3") {
                Type* componentType = typeSystem->getPrimitiveType("flt32");
                return typeSystem->getVectorType(componentType, 3);
            }
            if (simpleType->typeName == "fvec9") {
                Type* componentType = typeSystem->getPrimitiveType("flt32");
                return typeSystem->getVectorType(componentType, 9);
            }
            
            // Integer vector types (ivec2, ivec3, ivec9)
            if (simpleType->typeName == "ivec2") {
                Type* componentType = typeSystem->getPrimitiveType("int32");
                return typeSystem->getVectorType(componentType, 2);
            }
            if (simpleType->typeName == "ivec3") {
                Type* componentType = typeSystem->getPrimitiveType("int32");
                return typeSystem->getVectorType(componentType, 3);
            }
            if (simpleType->typeName == "ivec9") {
                Type* componentType = typeSystem->getPrimitiveType("int32");
                return typeSystem->getVectorType(componentType, 9);
            }
            
            // Try struct first
            Type* type = typeSystem->getStructType(simpleType->typeName);
            if (type) {
                return type;
            }
            
            // v0.2.39: Try enum type
            type = typeSystem->getEnumType(simpleType->typeName);
            if (type) {
                return type;
            }
            
            // any type — type-erased pointer (safe void* replacement)
            if (simpleType->typeName == "any") {
                return typeSystem->getAnyType();
            }

            // v0.2.36: dyn Trait types — fat pointer for dynamic dispatch
            if (simpleType->typeName.substr(0, 4) == "dyn ") {
                std::string traitName = simpleType->typeName.substr(4);
                return typeSystem->getDynTraitType(traitName);
            }
            
            // Try primitive type
            type = typeSystem->getPrimitiveType(simpleType->typeName);
            if (type && type->getKind() != TypeKind::ERROR) {
                return type;
            }
            
            {
                std::string errorMsg = "Unknown type: '" + simpleType->typeName + "'";
                std::string suggestion = findSimilarType(simpleType->typeName);
                if (!suggestion.empty()) {
                    errorMsg += ". Did you mean '" + suggestion + "'?";
                    // Add extra hint for common casing mistakes
                    std::string lower = simpleType->typeName;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    if (lower == suggestion || lower == "nil") {
                        errorMsg += " (Aria types are lowercase)";
                    }
                }
                addError(errorMsg, typeNode);
            }
            return typeSystem->getErrorType();
        }
        
        case ASTNode::NodeType::ARRAY_TYPE: {
            // Array type: int32[], int32[10], etc.
            aria::ArrayType* arrayType = static_cast<aria::ArrayType*>(typeNode);
            
            // Resolve element type
            Type* elementType = resolveTypeNode(arrayType->elementType.get());
            if (elementType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Determine array size
            int arraySize = -1;  // -1 indicates dynamic/unknown size (int32[])
            
            if (!arrayType->isDynamic && arrayType->sizeExpr) {
                // Fixed-size array (int32[10])
                // Need to evaluate constant expression for size
                // For now, if it's a literal, extract the value
                if (arrayType->sizeExpr->type == ASTNode::NodeType::LITERAL) {
                    LiteralExpr* sizeLit = static_cast<LiteralExpr*>(arrayType->sizeExpr.get());
                    
                    // Extract integer from variant
                    if (std::holds_alternative<int64_t>(sizeLit->value)) {
                        int64_t size64 = std::get<int64_t>(sizeLit->value);
                        if (size64 < 0) {
                            addError("Array size must be non-negative, got " + std::to_string(size64), typeNode);
                            return typeSystem->getErrorType();
                        }
                        arraySize = static_cast<int>(size64);
                    } else {
                        addError("Array size must be an integer literal", typeNode);
                        return typeSystem->getErrorType();
                    }
                } else {
                    // Try compile-time constant folding for expressions like 2+3, N*4, etc.
                    ComptimeValue constVal = constEvaluator->evaluate(arrayType->sizeExpr.get());
                    if (constVal.isInteger()) {
                        int64_t size64 = constVal.getInt();
                        if (size64 < 0) {
                            addError("Array size must be non-negative, got " + std::to_string(size64), typeNode);
                            return typeSystem->getErrorType();
                        }
                        arraySize = static_cast<int>(size64);
                    } else {
                        addError("Array size must be a constant integer expression", typeNode);
                        return typeSystem->getErrorType();
                    }
                }
            }
            
            // Create proper array type
            return typeSystem->getArrayType(elementType, arraySize);
        }
        
        case ASTNode::NodeType::POINTER_TYPE: {
            // Pointer type: int32@, string@, ?@ (erased), etc.
            aria::PointerType* ptrType = static_cast<aria::PointerType*>(typeNode);
            
            // ARIA-P3: ?-> / ?* — type-erased pointer
            // baseType is null; create an erased sema PointerType directly.
            if (ptrType->isErased) {
                return typeSystem->getErasedPointerType();
            }
            
            // Resolve base type
            Type* baseType = resolveTypeNode(ptrType->baseType.get());
            if (baseType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            return typeSystem->getPointerType(baseType);
        }
        
        case ASTNode::NodeType::SAFE_REF_TYPE: {
            // Safe reference type: int32$, string$, etc.
            // For now, treat safe references as pointer types in the type system
            // The borrow checker will enforce the safety rules
            aria::SafeRefType* safeRefType = static_cast<aria::SafeRefType*>(typeNode);
            
            // Resolve base type
            Type* baseType = resolveTypeNode(safeRefType->baseType.get());
            if (baseType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Use pointer type internally, borrow checker handles safety
            return typeSystem->getPointerType(baseType);
        }
        
        case ASTNode::NodeType::OPTIONAL_TYPE: {
            // Optional type: int64?, string?, etc.
            aria::OptionalTypeNode* optType = static_cast<aria::OptionalTypeNode*>(typeNode);
            
            // Resolve wrapped type
            Type* wrappedType = resolveTypeNode(optType->wrappedType.get());
            if (wrappedType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            return typeSystem->getOptionalType(wrappedType);
        }
        
        case ASTNode::NodeType::GENERIC_TYPE: {
            // Generic type instantiation: Box<int64>, Result<int32, string>, etc.
            aria::GenericType* genericType = static_cast<aria::GenericType*>(typeNode);
            
            // Phase 3.4: Generic Struct Instantiation (Session 13)
            // Resolve all type arguments
            std::vector<Type*> resolvedTypeArgs;
            TypeSubstitution substitution;
            
            for (const auto& typeArg : genericType->typeArgs) {
                Type* argType = resolveTypeNode(typeArg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
                resolvedTypeArgs.push_back(argType);
            }
            
            // Check for built-in generic types first
            if (genericType->baseName == "Result") {
                // Result<T> is a built-in type, not a struct
                if (resolvedTypeArgs.size() != 1) {
                    addError("Result<T> requires exactly 1 type argument, got " + 
                            std::to_string(resolvedTypeArgs.size()), typeNode);
                    return typeSystem->getErrorType();
                }
                
                // Use the built-in ResultType
                return typeSystem->getResultType(resolvedTypeArgs[0]);
            }
            
            // P1-3: Handle<T> - Generational memory handles for safe neurogenesis
            if (genericType->baseName == "Handle") {
                // Handle<T> is a built-in type for arena-based memory management
                if (resolvedTypeArgs.size() != 1) {
                    addError("Handle<T> requires exactly 1 type argument, got " + 
                            std::to_string(resolvedTypeArgs.size()), typeNode);
                    return typeSystem->getErrorType();
                }
                
                // Create HandleType wrapping the pointee type
                return typeSystem->getHandleType(resolvedTypeArgs[0]);
            }
            
            // P1-2: simd<T, N> - SIMD vectorization for performance
            if (genericType->baseName == "simd") {
                // simd<T, N> requires exactly 2 type arguments:
                // - T: element type (must be primitive for now)
                // - N: lane count (must be compile-time constant integer)
                if (genericType->typeArgs.size() != 2) {
                    addError("simd<T, N> requires exactly 2 type arguments, got " + 
                            std::to_string(genericType->typeArgs.size()), typeNode);
                    return typeSystem->getErrorType();
                }
                
                // First argument: element type
                Type* elementType = resolvedTypeArgs[0];
                
                // Validate element type is primitive (for now)
                if (!elementType->isPrimitive()) {
                    addError("simd<T, N> element type must be primitive, got: " + 
                            elementType->toString(), typeNode);
                    return typeSystem->getErrorType();
                }
                
                // Second argument: lane count (must be a simple type name representing an integer)
                // Support two formats:
                // 1. Plain integer: "4", "8", "16" (Phase 2: parser handles this)
                // 2. Identifier format: "N4", "N8", "N16" (Phase 1 workaround, still supported)
                aria::ASTNode* laneCountNode = genericType->typeArgs[1].get();
                size_t laneCount = 0;
                
                if (laneCountNode->type == ASTNode::NodeType::TYPE_ANNOTATION) {
                    aria::SimpleType* laneCountType = static_cast<aria::SimpleType*>(laneCountNode);
                    std::string laneCountStr = laneCountType->typeName;
                    
                    // Check for "N<number>" format (e.g., "N4", "N8", "N16") - Phase 1 workaround
                    if (laneCountStr.length() > 1 && laneCountStr[0] == 'N') {
                        laneCountStr = laneCountStr.substr(1);  // Strip 'N' prefix
                    }
                    // Otherwise, it should be a plain integer from the parser (Phase 2)
                    
                    try {
                        laneCount = std::stoull(laneCountStr);
                    } catch (...) {
                        addError("simd<T, N> lane count must be a positive integer (e.g., 4, 8, 16 or N4, N8, N16), got: " + 
                                laneCountType->typeName, typeNode);
                        return typeSystem->getErrorType();
                    }
                } else {
                    addError("simd<T, N> lane count must be a compile-time constant", typeNode);
                    return typeSystem->getErrorType();
                }
                
                // Validate lane count is valid (power of 2, reasonable range)
                if (laneCount == 0 || laneCount > 64) {
                    addError("simd<T, N> lane count must be between 1 and 64, got: " + 
                            std::to_string(laneCount), typeNode);
                    return typeSystem->getErrorType();
                }
                
                // Validate lane count is power of 2 (required for hardware SIMD alignment)
                if ((laneCount & (laneCount - 1)) != 0) {
                    addError("simd<T, N> lane count must be a power of 2 (1, 2, 4, 8, 16, 32, 64), got: " +
                            std::to_string(laneCount), typeNode);
                    return typeSystem->getErrorType();
                }
                
                // Create SimdType
                return typeSystem->getSimdType(elementType, laneCount);
            }
            
            if (genericType->baseName == "array" || genericType->baseName == "atomic" || 
                genericType->baseName == "Vec" || genericType->baseName == "HashMap") {
                // Built-in generic types - handle specially
                // For now, just return a placeholder struct type
                // The actual implementation will be in stdlib once fully integrated
                
                // atomic<T> requires special validation
                if (genericType->baseName == "atomic") {
                    // Validate argument count
                    if (resolvedTypeArgs.size() != 1) {
                        addError("atomic<T> requires exactly 1 type argument, got " + 
                                std::to_string(resolvedTypeArgs.size()), typeNode);
                        return typeSystem->getErrorType();
                    }
                    
                    // Validate type compatibility
                    Type* argType = resolvedTypeArgs[0];
                    if (!isAtomicCompatible(argType)) {
                        std::ostringstream msg;
                        msg << "atomic<T> requires lock-free compatible type (int8-64, uint8-64, bool, tbb8-64), got: "
                            << argType->toString();
                        addError(msg.str(), typeNode);
                        return typeSystem->getErrorType();
                    }
                    
                    // Create mangled name for atomic specialization
                    std::string mangledName = "atomic_" + argType->toString();
                    
                    // Check if we already have this specialization
                    Type* existingType = typeSystem->getStructType(mangledName);
                    if (existingType) {
                        return existingType;
                    }
                    
                    // Create atomic<T> struct type
                    // atomic<T> has: T value (aligned appropriately for atomic operations)
                    std::vector<StructType::Field> fields;
                    fields.push_back({"value", argType, 0});
                    
                    return typeSystem->createStructType(mangledName, fields);
                }
                
                // Vec<T> - Dynamic array (P0 feature for Nikola 0.1.0)
                // Maps to runtime AriaArray struct (see src/runtime/collections/collections.cpp)
                if (genericType->baseName == "Vec") {
                    // Validate argument count
                    if (resolvedTypeArgs.size() != 1) {
                        addError("Vec<T> requires exactly 1 type argument, got " + 
                                std::to_string(resolvedTypeArgs.size()), typeNode);
                        return typeSystem->getErrorType();
                    }
                    
                    Type* elemType = resolvedTypeArgs[0];
                    
                    // Create mangled name for Vec specialization
                    std::string mangledName = "Vec_" + elemType->toString();
                    
                    // Check if we already have this specialization
                    Type* existingType = typeSystem->getStructType(mangledName);
                    if (existingType) {
                        return existingType;
                    }
                    
                    // Create Vec<T> struct type matching AriaArray layout:
                    // typedef struct {
                    //     void* data;          // Element array (stored as wild int8*)
                    //     size_t length;       // Current count
                    //     size_t capacity;     // Allocated capacity
                    //     size_t element_size; // Bytes per element
                    //     int type_id;         // GC type tracking
                    // } AriaArray;
                    std::vector<StructType::Field> fields;
                    // Use wild int8* for data (generic pointer, matches C void*)
                    fields.push_back({"data", typeSystem->getPointerType(typeSystem->getPrimitiveType("int8")), 0});
                    fields.push_back({"length", typeSystem->getPrimitiveType("int64"), 8});
                    fields.push_back({"capacity", typeSystem->getPrimitiveType("int64"), 16});
                    fields.push_back({"element_size", typeSystem->getPrimitiveType("int64"), 24});
                    fields.push_back({"type_id", typeSystem->getPrimitiveType("int32"), 32});
                    
                    return typeSystem->createStructType(mangledName, fields);
                }
                
                // HashMap<K,V> - Hash table (P0 feature for Nikola 0.1.0)
                // Maps to runtime AriaMap struct (see src/runtime/collections/map.cpp)
                if (genericType->baseName == "HashMap") {
                    // Validate argument count - HashMap requires TWO type arguments (key, value)
                    if (resolvedTypeArgs.size() != 2) {
                        addError("HashMap<K,V> requires exactly 2 type arguments, got " + 
                                std::to_string(resolvedTypeArgs.size()), typeNode);
                        return typeSystem->getErrorType();
                    }
                    
                    Type* keyType = resolvedTypeArgs[0];
                    Type* valueType = resolvedTypeArgs[1];
                    
                    // Create mangled name for HashMap specialization
                    std::string mangledName = "HashMap_" + keyType->toString() + "_" + valueType->toString();
                    
                    // Check if we already have this specialization
                    Type* existingType = typeSystem->getStructType(mangledName);
                    if (existingType) {
                        return existingType;
                    }
                    
                    // Create HashMap<K,V> struct type matching AriaMap layout:
                    // typedef struct {
                    //     AriaMapEntry* entries;  // Array of entries (stored as wild int8*)
                    //     size_t capacity;        // Total slots (power of 2)
                    //     size_t length;          // Number of occupied entries
                    //     size_t key_size;        // Size of keys in bytes
                    //     size_t value_size;      // Size of values in bytes
                    //     float load_factor;      // Max load before resize
                    // } AriaMap;
                    std::vector<StructType::Field> fields;
                    // Use wild int8* for entries (generic pointer, matches C void*)
                    fields.push_back({"entries", typeSystem->getPointerType(typeSystem->getPrimitiveType("int8")), 0});
                    fields.push_back({"capacity", typeSystem->getPrimitiveType("int64"), 8});
                    fields.push_back({"length", typeSystem->getPrimitiveType("int64"), 16});
                    fields.push_back({"key_size", typeSystem->getPrimitiveType("int64"), 24});
                    fields.push_back({"value_size", typeSystem->getPrimitiveType("int64"), 32});
                    fields.push_back({"load_factor", typeSystem->getPrimitiveType("flt32"), 40});
                    
                    return typeSystem->createStructType(mangledName, fields);
                }
                
                // Create a mangled name for this instantiation
                std::string mangledName = genericType->baseName;
                for (const auto& argType : resolvedTypeArgs) {
                    mangledName += "_" + argType->toString();
                }
                
                // Check if we already have this specialization
                Type* existingType = typeSystem->getStructType(mangledName);
                if (existingType) {
                    return existingType;
                }
                
                // Create a new struct type for this specialization
                // This is a placeholder - actual fields will be defined in stdlib
                std::vector<StructType::Field> fields;
                if (genericType->baseName == "array") {
                    // array<T> has: T* data, int64 length, int64 capacity
                    Type* elemType = resolvedTypeArgs[0];
                    Type* ptrType = typeSystem->getPointerType(elemType);
                    fields.push_back({"data", ptrType, 0});
                    fields.push_back({"length", typeSystem->getPrimitiveType("int64"), 8});
                    fields.push_back({"capacity", typeSystem->getPrimitiveType("int64"), 16});
                }
                
                return typeSystem->createStructType(mangledName, fields);
            }
            
            // P1-5 Phase 3: Dimensional Type Support
            // Check for dimensional types: fix256<Joules>, fix128<Meters>, etc.
            // Pattern: BaseType<DimensionName> where:
            // - BaseType is a numeric type (fix256, fix128, int64, etc.)
            // - DimensionName is a registered dimension (Joules, Meters, etc.)
            if (genericType->typeArgs.size() == 1) {
                // Check if the type argument is a simple identifier (potential dimension name)
                aria::ASTNode* typeArgNode = genericType->typeArgs[0].get();
                if (typeArgNode->type == ASTNode::NodeType::TYPE_ANNOTATION) {
                    aria::SimpleType* simpleTypeArg = static_cast<aria::SimpleType*>(typeArgNode);
                    std::string potentialDimensionName = simpleTypeArg->typeName;
                    
                    // Check if this is a registered dimension
                    const Dimension* dim = typeSystem->lookupDimension(potentialDimensionName);
                    if (dim != nullptr) {
                        // This is a dimensional type! Get the base numeric type
                        // Try primitive types first (int64, flt64, etc.)
                        Type* baseType = typeSystem->getPrimitiveType(genericType->baseName);
                        
                        // If not primitive, try struct types (fix256, etc.)
                        if (!baseType) {
                            baseType = typeSystem->getStructType(genericType->baseName);
                        }
                        
                        if (!baseType) {
                            addError("Unknown base type for dimensional: '" + 
                                   genericType->baseName + "' in " + 
                                   genericType->baseName + "<" + potentialDimensionName + ">", 
                                   typeNode);
                            return typeSystem->getErrorType();
                        }
                        
                        // Validate that base type is numeric
                        if (!isNumericType(baseType)) {
                            addError("Dimensional types require numeric base type, got: " + 
                                   baseType->toString(), typeNode);
                            return typeSystem->getErrorType();
                        }
                        
                        // Create and return dimensional type
                        return typeSystem->getDimensionalType(baseType, *dim, potentialDimensionName);
                    }
                }
            }
            
            // Look up the generic struct declaration
            auto it = genericStructRegistry.find(genericType->baseName);
            if (it == genericStructRegistry.end()) {
                // Not a generic struct - check if it's a regular struct
                Type* baseStructType = typeSystem->getStructType(genericType->baseName);
                if (baseStructType) {
                    addError("'" + genericType->baseName + "' is not a generic type", typeNode);
                } else {
                    addError("Unknown generic type: '" + genericType->baseName + "'", typeNode);
                }
                return typeSystem->getErrorType();
            }
            
            StructDeclStmt* genericStructDecl = it->second;
            
            // Validate type argument count
            if (resolvedTypeArgs.size() != genericStructDecl->genericParams.size()) {
                std::ostringstream msg;
                msg << "Generic type '" << genericType->baseName << "' expects "
                    << genericStructDecl->genericParams.size() << " type arguments, got "
                    << resolvedTypeArgs.size();
                addError(msg.str(), typeNode);
                return typeSystem->getErrorType();
            }
            
            // Build type substitution map
            for (size_t i = 0; i < genericStructDecl->genericParams.size(); i++) {
                substitution[genericStructDecl->genericParams[i].name] = resolvedTypeArgs[i];
            }
            
            // Check if monomorphizer is available
            if (!monomorphizer) {
                addError("Internal error: monomorphizer not initialized for generic type '" + 
                        genericType->baseName + "<...>'. This is a compiler bug.", typeNode);
                return typeSystem->getErrorType();
            }
            
            // Request monomorphization
            std::string mangledName = monomorphizer->requestStructSpecialization(
                genericStructDecl, substitution);
            
            if (mangledName.empty()) {
                // Error occurred during monomorphization
                if (monomorphizer->hasErrors()) {
                    for (const auto& err : monomorphizer->getErrors()) {
                        addError(err.message, err.line, err.column);
                    }
                }
                return typeSystem->getErrorType();
            }
            
            // Look up or create the specialized struct type
            Type* specializedType = typeSystem->getStructType(mangledName);
            if (!specializedType) {
                // requestStructSpecialization already registers the type via
                // typeSystem->createStructType(). If getStructType still returns
                // nullptr, the monomorphizer hit an internal issue (unknown field
                // type, failed clone, etc.). Report as internal error.
                addError("Internal error: monomorphizer returned mangled name '" +
                        mangledName + "' but type was not registered in TypeSystem. "
                        "Check for circular type dependencies or unknown field types.", typeNode);
                return typeSystem->getErrorType();
            }
            
            return specializedType;
        }
        
        case ASTNode::NodeType::FUNCTION_TYPE: {
            // Function pointer type: (retType)(paramTypes...)
            // Return a placeholder primitive type so the type checker doesn't error.
            // The IR generator handles FUNCTION_TYPE specially via typeNode check
            // before stmt->typeName is used, so this placeholder is never lowered.
            Type* placeholder = typeSystem->getPrimitiveType("uint64");
            if (!placeholder) placeholder = typeSystem->getPrimitiveType("int64");
            return placeholder ? placeholder : typeSystem->getErrorType();
        }

        default:
            addError("Unsupported type syntax in type resolution. "
                    "Valid type forms: Type, Type[], Type@, Type<Args>, Result<T>, (Ret)(Params)", typeNode);
            return typeSystem->getErrorType();
    }
}

// ============================================================================
// Variable Declaration Type Checking
// ============================================================================

void TypeChecker::checkVarDecl(VarDeclStmt* stmt) {
    // Prevent shadowing reserved builtin names
    static const std::unordered_set<std::string> reservedBuiltins = {
        "ok", "print", "println", "stdout_write", "stderr_write",
        "to_string", "fail", "pass", "drop", "raw", "sleep_ms", "exit", "env_get", "sort_lines",
        "sys"
    };
    if (reservedBuiltins.count(stmt->varName)) {
        addError("'" + stmt->varName + "' is a reserved builtin and cannot be used as a variable name", stmt);
        return;
    }

    // ========================================================================
    // Borrow Qualifier Validation (v0.2.35)
    // $$i = immutable borrow, $$m = mutable borrow
    // ========================================================================
    if (stmt->isBorrowImm || stmt->isBorrowMut) {
        // Cannot have both $$i and $$m
        if (stmt->isBorrowImm && stmt->isBorrowMut) {
            addError("Cannot use both $$i (immutable borrow) and $$m (mutable borrow) on the same variable", stmt);
            return;
        }
        // Cannot combine borrow with wild/wildx
        if (stmt->isWild || stmt->isWildx) {
            addError("Borrow qualifiers ($$i/$$m) cannot be combined with wild/wildx — "
                     "borrows reference existing memory, they don't allocate", stmt);
            return;
        }
        // Borrow must have an initializer (what are we borrowing?)
        if (!stmt->initializer) {
            addError("Borrow variable '" + stmt->varName + "' must have an initializer — "
                     "$$i/$$m must reference an existing variable", stmt);
            return;
        }
        auto isLiteralIndexBorrowInitializer = [](ASTNode* node) -> bool {
            if (!node || node->type != ASTNode::NodeType::INDEX) {
                return false;
            }
            auto* indexExpr = static_cast<IndexExpr*>(node);
            if (!indexExpr->array || indexExpr->array->type != ASTNode::NodeType::IDENTIFIER) {
                return false;
            }
            if (!indexExpr->index || indexExpr->index->type != ASTNode::NodeType::LITERAL) {
                return false;
            }
            auto* literal = static_cast<LiteralExpr*>(indexExpr->index.get());
            return std::holds_alternative<int64_t>(literal->value);
        };

        // Borrow initializer must be addressable storage. Start conservatively:
        // variables, struct fields, and direct literal-index fixed-array slots.
        if (stmt->initializer->type != ASTNode::NodeType::IDENTIFIER &&
            stmt->initializer->type != ASTNode::NodeType::MEMBER_ACCESS &&
            !isLiteralIndexBorrowInitializer(stmt->initializer.get())) {
            addError("Borrow variable '" + stmt->varName + "' must borrow from addressable storage "
                     "(a named variable, struct field, or direct literal-index array element)", stmt);
            return;
        }
    }

    Type* declaredType = nullptr;
    
    // Handle new typeNode or legacy typeName
    if (stmt->typeNode) {
        // Resolve type from AST node (supports arrays, pointers, etc.)
        declaredType = resolveTypeNode(stmt->typeNode.get());
        if (!declaredType || declaredType->getKind() == TypeKind::ERROR) {
            addError("Cannot resolve type in variable declaration", stmt);
            return;
        }
        
        // Array size inference: int32[]:x = [1, 2, 3] → narrow to int32[3]
        if (declaredType->getKind() == TypeKind::ARRAY) {
            const ArrayType* declaredArray = static_cast<const ArrayType*>(declaredType);
            if (declaredArray->isDynamic() && stmt->initializer &&
                stmt->initializer->type == ASTNode::NodeType::ARRAY_LITERAL) {
                auto* arrayLit = static_cast<ArrayLiteralExpr*>(stmt->initializer.get());
                int inferredSize = static_cast<int>(arrayLit->elements.size());
                declaredType = typeSystem->getArrayType(
                    const_cast<Type*>(declaredArray->getElementType()), inferredSize);
            }
        }

        // CRITICAL: Update typeName to resolved type name for IR generation
        // For generic structs, this will be the mangled name (_Aria_M_Box_<hash>_int64)
        // For other types, this will be the canonical name (int64, string, etc.)
        stmt->typeName = declaredType->toString();

        // FUNCTION_TYPE: function pointer variable — IR generator handles all codegen.
        // Register as unknown (so call sites don't error) and skip all type checks.
        if (stmt->typeNode->type == ASTNode::NodeType::FUNCTION_TYPE) {
            symbolTable->defineSymbol(stmt->varName,
                                     SymbolKind::VARIABLE,
                                     typeSystem->getUnknownType(),
                                     stmt->line, stmt->column);
            return;
        }
    } else {
        // Legacy path: Get declared type from typeName string
        // Try struct first to avoid auto-creating primitive types
        declaredType = typeSystem->getStructType(stmt->typeName);
        
        if (!declaredType) {
            // Try primitive type
            declaredType = typeSystem->getPrimitiveType(stmt->typeName);
            
            if (!declaredType || declaredType->getKind() == TypeKind::ERROR) {
                std::string errorMsg = "Unknown type: '" + stmt->typeName + "'";
                std::string suggestion = findSimilarType(stmt->typeName);
                if (!suggestion.empty()) {
                    errorMsg += ". Did you mean '" + suggestion + "'?";
                    std::string lower = stmt->typeName;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    if (lower == suggestion || lower == "nil") {
                        errorMsg += " (Aria types are lowercase)";
                    }
                }
                addError(errorMsg, stmt);
                return;
            }
        }
    }
    
    // ARIA-024: int128/int256/int512 now use Limb-Based Integral Model (LBIM)
    // The semantic block for these types has been removed as they are now
    // represented as structs of i64 limbs, bypassing LLVM IPSCCP bugs.

    // Check const variables have initializers
    if (stmt->isConst && !stmt->initializer) {
        addError("const variable '" + stmt->varName + "' must have initializer", stmt);
        return;
    }
    
    // If initializer exists, check type compatibility
    if (stmt->initializer) {
        Type* initType = inferType(stmt->initializer.get());
        
        if (initType->getKind() == TypeKind::ERROR) {
            // Error already reported by inferType
            return;
        }
        
        // ========================================================================
        // NIL vs NULL Type Safety Enforcement
        // NIL can ONLY be used with optional types (T?)
        // NULL can ONLY be used with pointer types (T@)
        // ========================================================================
        if (stmt->initializer->type == ASTNode::NodeType::LITERAL) {
            LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
            
            // Check NIL literal - must be used with optional types only
            if (literal->explicit_type == "NIL") {
                if (declaredType->getKind() != TypeKind::OPTIONAL) {
                    addError("NIL can only be assigned to optional types (T?). " 
                            "Variable '" + stmt->varName + "' has type '" + 
                            declaredType->toString() + "'. Use NULL for pointers.", stmt);
                    return;
                }
            }
            
            // Check NULL literal - must be used with pointer types only
            if (literal->explicit_type == "NULL") {
                if (declaredType->getKind() != TypeKind::POINTER) {
                    addError("NULL can only be assigned to pointer types (T@). " 
                            "Variable '" + stmt->varName + "' has type '" + 
                            declaredType->toString() + "'. Use NIL for optionals.", stmt);
                    return;
                }
            }
        }
        
        // TBB Type Validation (Phase 3.2.4)
        // Special handling for integer values (literals or expressions) assigned to TBB types
        // Allows: tbb8:x = 100; and tbb8:x = 10 + 20;
        // This is safe because:
        //   1. int32 has smaller range than most TBB types (except tbb8)
        //   2. Range check will validate at compile time if possible
        //   3. Runtime check inserted if value not known at compile time
        bool tbbLiteralAssignment = false;
        if (isTBBType(declaredType)) {
            // Allow int32 → tbb coercion for literals and simple expressions
            // This enables natural syntax: tbb8:x = 42;
            if (initType->toString() == "int32" || initType->toString() == "int64") {
                // Check if it's a compile-time constant literal
                if (stmt->initializer->type == ASTNode::NodeType::LITERAL) {
                    LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
                    if (std::holds_alternative<int64_t>(literal->value)) {
                        int64_t value = std::get<int64_t>(literal->value);
                        checkTBBLiteralValue(value, declaredType, stmt);
                        tbbLiteralAssignment = true;
                        
                        // If no errors, allow the assignment (literal is in range)
                        if (hasErrors()) {
                            return;  // Validation failed
                        }
                    }
                } else {
                    // Expression result (e.g., 10 + 20) — try compile-time constant folding
                    ComptimeValue constVal = constEvaluator->evaluate(stmt->initializer.get());
                    if (constVal.isInteger()) {
                        int64_t value = constVal.getInt();
                        checkTBBLiteralValue(value, declaredType, stmt);
                        tbbLiteralAssignment = true;
                        if (hasErrors()) {
                            return;  // Range validation failed
                        }
                    } else {
                        // Non-constant expression — allow with runtime range enforcement
                        tbbLiteralAssignment = true;
                    }
                }
            }
        }
        
        // ERR Literal Validation
        // Special handling for ERR literals assigned to TBB types
        bool errLiteralAssignment = false;
        if (isTBBType(declaredType) && stmt->initializer->type == ASTNode::NodeType::LITERAL) {
            LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
            if (std::holds_alternative<std::string>(literal->value) && 
                std::get<std::string>(literal->value) == "ERR") {
                // ERR is valid for any TBB type
                // The actual value (-128 for tbb8, -32768 for tbb16, etc.) will be 
                // determined in IR generation based on the target type
                errLiteralAssignment = true;
                // No validation needed - ERR is always valid for TBB types
            }
        }
        
        // Balanced Type Validation (Phase 3.2.5)
        // Special handling for integer literals assigned to balanced types
        bool balancedLiteralAssignment = false;
        if (isBalancedType(declaredType)) {
            // Check for direct literal
            if (stmt->initializer->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
                if (std::holds_alternative<int64_t>(literal->value)) {
                    int64_t value = std::get<int64_t>(literal->value);
                    checkBalancedLiteralValue(value, declaredType, stmt);
                    balancedLiteralAssignment = true;
                    
                    // If no errors, allow the assignment (literal is in range)
                    if (hasErrors()) {
                        return;  // Validation failed
                    }
                }
            }
            // Check for negative literal (unary minus applied to literal)
            else if (stmt->initializer->type == ASTNode::NodeType::UNARY_OP) {
                UnaryExpr* unary = static_cast<UnaryExpr*>(stmt->initializer.get());
                if (unary->op.type == frontend::TokenType::TOKEN_MINUS &&
                    unary->operand->type == ASTNode::NodeType::LITERAL) {
                    LiteralExpr* literal = static_cast<LiteralExpr*>(unary->operand.get());
                    if (std::holds_alternative<int64_t>(literal->value)) {
                        int64_t value = -std::get<int64_t>(literal->value);  // Negate
                        checkBalancedLiteralValue(value, declaredType, stmt);
                        balancedLiteralAssignment = true;
                        
                        // If no errors, allow the assignment (literal is in range)
                        if (hasErrors()) {
                            return;  // Validation failed
                        }
                    }
                }
            }
        }
        
        // Numeric Exotic Type Validation (Phase 3.3)  
        // Special handling for integer literals assigned to frac*, tfp*, vec9, etc.
        bool numericExoticLiteralAssignment = false;
        if (isNumericExoticType(declaredType)) {
            // Check for direct literal
            if (stmt->initializer->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
                if (std::holds_alternative<int64_t>(literal->value) || 
                    std::holds_alternative<double>(literal->value)) {
                    // Allow integer or float literals to initialize exotic numeric types
                    // The IR generator will create the appropriate struct representation
                    numericExoticLiteralAssignment = true;
                }
            }
            // Check for negative literal (unary minus applied to literal)
            else if (stmt->initializer->type == ASTNode::NodeType::UNARY_OP) {
                UnaryExpr* unary = static_cast<UnaryExpr*>(stmt->initializer.get());
                if (unary->op.type == frontend::TokenType::TOKEN_MINUS &&
                    unary->operand->type == ASTNode::NodeType::LITERAL) {
                    LiteralExpr* literal = static_cast<LiteralExpr*>(unary->operand.get());
                    if (std::holds_alternative<int64_t>(literal->value) ||
                        std::holds_alternative<double>(literal->value)) {
                        // Allow negative literals for exotic numeric types
                        numericExoticLiteralAssignment = true;
                    }
                }
            }
        }
        
        // High-Precision Literal Conversion (Phase 3.2.5)
        // Handle raw literal strings when target type is known
        bool highPrecisionLiteralAssignment = false;
        if (stmt->initializer->type == ASTNode::NodeType::LITERAL) {
            LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
            if (literal->hasRawValue()) {
                // We have a high-precision literal and know the target type
                // Validate the conversion is possible
                const std::string& raw = literal->getRawValue();
                std::string typeName = declaredType->toString();
                
                // Check if target type is a high-precision type
                if (typeName == "flt128" || typeName == "flt256" || typeName == "flt512" ||
                    typeName == "int128" || typeName == "int256" || typeName == "int512" ||
                    typeName == "uint128" || typeName == "uint256" || typeName == "uint512") {
                    
                    // Validate that the literal can be converted
                    // The actual conversion will happen in IR generation
                    bool is_float = (raw.find('.') != std::string::npos || 
                                    raw.find('e') != std::string::npos ||
                                    raw.find('E') != std::string::npos);
                    
                    bool type_is_float = (typeName.find("flt") == 0);
                    
                    if (is_float && !type_is_float) {
                        addError("Cannot assign float literal to integer type '" + typeName + "'", stmt);
                        return;
                    }
                    
                    if (!is_float && type_is_float) {
                        // Integer to float is okay (will be converted)
                    }
                    
                    highPrecisionLiteralAssignment = true;
                }
            }
        }
        
        // Standard Integer Type Validation
        // Special handling for integer literals assigned to smaller integer types
        // This allows safe narrowing conversions when the literal value fits at compile time
        bool standardIntLiteralAssignment = false;
        if (isStandardIntType(declaredType)) {
            // Check if initializer is a direct literal
            if (stmt->initializer->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
                if (std::holds_alternative<int64_t>(literal->value)) {
                    int64_t value = std::get<int64_t>(literal->value);
                    if (canLiteralFitInIntType(value, declaredType, stmt)) {
                        standardIntLiteralAssignment = true;
                        // Literal fits in target type, allow the assignment
                    } else {
                        return;  // Validation failed, error already reported
                    }
                }
            }
            // Check if initializer is a unary minus on a literal (negative number)
            else if (stmt->initializer->type == ASTNode::NodeType::UNARY_OP) {
                UnaryExpr* unaryExpr = static_cast<UnaryExpr*>(stmt->initializer.get());
                if (unaryExpr->op.type == TokenType::TOKEN_MINUS && 
                    unaryExpr->operand->type == ASTNode::NodeType::LITERAL) {
                    LiteralExpr* literal = static_cast<LiteralExpr*>(unaryExpr->operand.get());
                    if (std::holds_alternative<int64_t>(literal->value)) {
                        int64_t value = -std::get<int64_t>(literal->value);  // Apply negation
                        if (canLiteralFitInIntType(value, declaredType, stmt)) {
                            standardIntLiteralAssignment = true;
                            // Literal fits in target type, allow the assignment
                        } else {
                            return;  // Validation failed, error already reported
                        }
                    }
                }
            }
        }
        
        // Array Literal Integer Coercion
        // Special handling for array literals with integer elements assigned to different integer array types
        // Example: int64[]:nums = [10, 20, 30];  // Literals are int32, but should fit in int64
        bool arrayLiteralIntCoercion = false;
        if (declaredType->getKind() == TypeKind::POINTER && 
            initType->getKind() == TypeKind::POINTER &&
            stmt->initializer->type == ASTNode::NodeType::ARRAY_LITERAL) {
            
            const PointerType* declaredPtr = static_cast<const PointerType*>(declaredType);
            const PointerType* initPtr = static_cast<const PointerType*>(initType);
            
            Type* declaredElem = declaredPtr->getPointeeType();
            Type* initElem = initPtr->getPointeeType();
            
            // Check if both element types are standard integers
            if (isStandardIntType(declaredElem) && isStandardIntType(initElem)) {
                ArrayLiteralExpr* arrayLit = static_cast<ArrayLiteralExpr*>(stmt->initializer.get());
                
                // Check if all elements are integer literals that fit in the declared element type
                bool allFit = true;
                for (const auto& elem : arrayLit->elements) {
                    if (elem->type != ASTNode::NodeType::LITERAL) {
                        allFit = false;
                        break;
                    }
                    
                    LiteralExpr* literal = static_cast<LiteralExpr*>(elem.get());
                    if (!std::holds_alternative<int64_t>(literal->value)) {
                        allFit = false;
                        break;
                    }
                    
                    int64_t value = std::get<int64_t>(literal->value);
                    if (!integerFitsInType(value, declaredElem->toString())) {
                        allFit = false;
                        break;
                    }
                }
                
                if (allFit) {
                    arrayLiteralIntCoercion = true;
                    // All literals fit, allow the assignment
                }
            }
        }
        
        // P2.7: Array Literal Object Literal Coercion
        // Special handling for array literals with object literal elements assigned to struct array types
        // Example: Point[2]:points = [{x: 1, y: 2}, {x: 3, y: 4}];
        bool arrayLiteralObjectCoercion = false;
        if (declaredType->getKind() == TypeKind::ARRAY &&
            stmt->initializer->type == ASTNode::NodeType::ARRAY_LITERAL) {
            
            const ArrayType* declaredArray = static_cast<const ArrayType*>(declaredType);
            Type* declaredElem = declaredArray->getElementType();
            
            // Check if element type is a struct
            if (declaredElem->getKind() == TypeKind::STRUCT) {
                ArrayLiteralExpr* arrayLit = static_cast<ArrayLiteralExpr*>(stmt->initializer.get());
                const StructType* structType = static_cast<const StructType*>(declaredElem);
                
                // Check if all array elements are object literals
                bool allObjectLiterals = true;
                for (const auto& elem : arrayLit->elements) {
                    if (elem->type != ASTNode::NodeType::OBJECT_LITERAL) {
                        allObjectLiterals = false;
                        break;
                    }
                }
                
                if (allObjectLiterals) {
                    // Mark each object literal with the struct type name
                    const auto& structFields = structType->getFields();
                    
                    for (const auto& elem : arrayLit->elements) {
                        ObjectLiteralExpr* objLit = static_cast<ObjectLiteralExpr*>(elem.get());
                        
                        // Validate field count
                        if (objLit->fields.size() != structFields.size()) {
                            addError("Object literal in array has " + std::to_string(objLit->fields.size()) + 
                                    " fields, but struct '" + structType->getName() + 
                                    "' requires " + std::to_string(structFields.size()) + " fields", elem.get());
                            return;
                        }
                        
                        // Validate each field's type
                        for (size_t i = 0; i < objLit->fields.size(); ++i) {
                            const auto& litField = objLit->fields[i];
                            const auto& structField = structFields[i];
                            
                            // Infer the field value's type
                            Type* fieldValueType = inferType(litField.value.get());
                            if (fieldValueType->getKind() == TypeKind::ERROR) {
                                return;  // Error already reported
                            }
                            
                            // Check type compatibility
                            if (!fieldValueType->isAssignableTo(structField.type) && 
                                !canCoerce(fieldValueType, structField.type)) {
                                addError("Field '" + structField.name + "' of struct '" + 
                                        structType->getName() + "' in array element expects type '" + 
                                        structField.type->toString() + "', but got '" + 
                                        fieldValueType->toString() + "'", litField.value.get());
                                return;
                            }
                        }
                        
                        // Mark the object literal with the struct type name for IR generation
                        objLit->type_name = structType->getName();
                    }
                    
                    arrayLiteralObjectCoercion = true;
                }
            }
        }
        
        // Optional Type Integer Literal Coercion
        // Special handling for integer literals assigned to optional integer types
        // Example: int64?:opt = 42;  // 42 is int32, but should fit in int64
        bool optionalIntLiteralCoercion = false;
        if (declaredType->getKind() == TypeKind::OPTIONAL && 
            stmt->initializer->type == ASTNode::NodeType::LITERAL) {
            
            const OptionalType* declaredOptional = static_cast<const OptionalType*>(declaredType);
            Type* wrappedType = declaredOptional->getWrappedType();
            
            if (isStandardIntType(wrappedType)) {
                LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
                if (std::holds_alternative<int64_t>(literal->value)) {
                    int64_t value = std::get<int64_t>(literal->value);
                    if (integerFitsInType(value, wrappedType->toString())) {
                        optionalIntLiteralCoercion = true;
                        // Literal fits in wrapped type, allow the assignment
                    }
                }
            }
        }
        
        // Object Literal Struct Initialization (Phase 3.4)
        // Special handling for positional object literals assigned to struct-like types
        // This includes: frac8/16/32/64 (3 fields), tfp32/64 (2 fields), vec9 (9 fields), etc.
        // Example: frac8:half = {0, 1, 2};  // {whole, num, denom}
        bool objectLiteralStructAssignment = false;
        if (stmt->initializer->type == ASTNode::NodeType::OBJECT_LITERAL) {
            ObjectLiteralExpr* objLit = static_cast<ObjectLiteralExpr*>(stmt->initializer.get());
            
            // Check if declared type is a struct type
            if (declaredType->getKind() == TypeKind::STRUCT) {
                const StructType* structType = static_cast<const StructType*>(declaredType);
                
                // Get struct fields
                const std::vector<StructType::Field>& structFields = structType->getFields();
                
                // Check field count matches
                if (objLit->fields.size() != structFields.size()) {
                    addError("Object literal has " + std::to_string(objLit->fields.size()) + 
                            " fields, but struct '" + structType->getName() + 
                            "' requires " + std::to_string(structFields.size()) + " fields", stmt);
                    return;
                }
                
                // Validate each field's type
                for (size_t i = 0; i < objLit->fields.size(); ++i) {
                    const auto& litField = objLit->fields[i];
                    const auto& structField = structFields[i];
                    
                    // P2.7: Special handling for array literal fields with object literal elements
                    // If field expects array of structs, mark object literals with struct type name
                    if (litField.value->type == ASTNode::NodeType::ARRAY_LITERAL &&
                        structField.type->getKind() == TypeKind::ARRAY) {
                        
                        const ArrayType* expectedArrayType = static_cast<const ArrayType*>(structField.type);
                        Type* expectedElemType = expectedArrayType->getElementType();
                        
                        if (expectedElemType->getKind() == TypeKind::STRUCT) {
                            const StructType* expectedStructType = static_cast<const StructType*>(expectedElemType);
                            ArrayLiteralExpr* arrayLit = static_cast<ArrayLiteralExpr*>(litField.value.get());
                            
                            // Mark each object literal element with the struct type name
                            for (const auto& elem : arrayLit->elements) {
                                if (elem->type == ASTNode::NodeType::OBJECT_LITERAL) {
                                    ObjectLiteralExpr* elemObjLit = static_cast<ObjectLiteralExpr*>(elem.get());
                                    elemObjLit->type_name = expectedStructType->getName();
                                }
                            }
                        }
                    }
                    
                    // Infer the field value's type
                    Type* fieldValueType = inferType(litField.value.get());
                    if (fieldValueType->getKind() == TypeKind::ERROR) {
                        return;  // Error already reported
                    }
                    
                    // Check type compatibility (allow int32/int64 literals)
                    bool compatible = false;
                    if (fieldValueType->isAssignableTo(structField.type)) {
                        compatible = true;
                    } else if (canCoerce(fieldValueType, structField.type)) {
                        compatible = true;
                    } else if ((fieldValueType->toString() == "int32" || fieldValueType->toString() == "int64") &&
                               isStandardIntType(structField.type)) {
                        // Allow integer literal coercion for standard int struct fields
                        if (litField.value->type == ASTNode::NodeType::LITERAL) {
                            LiteralExpr* lit = static_cast<LiteralExpr*>(litField.value.get());
                            if (std::holds_alternative<int64_t>(lit->value)) {
                                int64_t value = std::get<int64_t>(lit->value);
                                compatible = integerFitsInType(value, structField.type->toString());
                            }
                        }
                    } else if ((fieldValueType->toString() == "int32" || fieldValueType->toString() == "int64") &&
                               isBalancedType(structField.type)) {
                        // Allow integer literal coercion for balanced type struct fields (trit, nit, tryte, nyte)
                        // Only when the value is unambiguous (fits in the target type's range)
                        
                        // Handle both direct literals and unary negation of literals
                        int64_t value = 0;
                        bool hasValue = false;
                        
                        if (litField.value->type == ASTNode::NodeType::LITERAL) {
                            LiteralExpr* lit = static_cast<LiteralExpr*>(litField.value.get());
                            if (std::holds_alternative<int64_t>(lit->value)) {
                                value = std::get<int64_t>(lit->value);
                                hasValue = true;
                            }
                        } else if (litField.value->type == ASTNode::NodeType::UNARY_OP) {
                            // Handle unary negation: -2, -1, etc.
                            UnaryExpr* unary = static_cast<UnaryExpr*>(litField.value.get());
                            if (unary->op.type == TokenType::TOKEN_MINUS && unary->operand->type == ASTNode::NodeType::LITERAL) {
                                LiteralExpr* lit = static_cast<LiteralExpr*>(unary->operand.get());
                                if (std::holds_alternative<int64_t>(lit->value)) {
                                    value = -std::get<int64_t>(lit->value);  // Negate the literal
                                    hasValue = true;
                                }
                            }
                        }
                        
                        if (hasValue) {
                            // Check if value fits in the target balanced type
                            if (isNitType(structField.type)) {
                                // nit: [-4, 4] or -128 (ERR sentinel)
                                compatible = ((value >= -4 && value <= 4) || value == -128);
                            } else if (isTritType(structField.type)) {
                                // trit: {-1, 0, 1} or -128 (ERR sentinel)
                                compatible = (value == -1 || value == 0 || value == 1 || value == -128);
                            } else if (isNyteType(structField.type) || isTryteType(structField.type)) {
                                // tryte/nyte: [-29524, 29524]
                                compatible = (value >= -29524 && value <= 29524);
                            }
                        }
                    }
                    
                    if (!compatible) {
                        addError("Field '" + structField.name + "' of struct '" + 
                                structType->getName() + "' expects type '" + 
                                structField.type->toString() + "', but got '" + 
                                fieldValueType->toString() + "'", litField.value.get());
                        return;
                    }
                }
                
                // Mark the object literal with the struct type name for IR generation
                objLit->type_name = structType->getName();
                
                // Validation passed
                objectLiteralStructAssignment = true;
            }
            // Check if declared type is an exotic numeric type with struct-like initialization
            else if (isNumericExoticType(declaredType) || isBalancedType(declaredType) || isTBBType(declaredType)) {
                PrimitiveType* primType = dynamic_cast<PrimitiveType*>(declaredType);
                if (primType) {
                    const std::string& typeName = primType->getName();
                    
                    // Determine expected field count based on type
                    size_t expectedFieldCount = 0;
                    if (typeName.find("frac") == 0) {
                        expectedFieldCount = 3;  // {whole, num, denom}
                    } else if (typeName.find("tfp") == 0) {
                        expectedFieldCount = 2;  // {exponent, mantissa}
                    } else if (typeName == "vec9" || typeName == "dvec9") {
                        expectedFieldCount = 9;  // 9 components
                    } else if (typeName == "tmatrix") {
                        // tmatrix can have variable size - just accept any field count for now
                        expectedFieldCount = objLit->fields.size();
                    } else if (typeName == "ttensor") {
                        // ttensor can have variable size - just accept any field count for now
                        expectedFieldCount = objLit->fields.size();
                    }
                    
                    // Check field count
                    if (expectedFieldCount > 0 && objLit->fields.size() != expectedFieldCount) {
                        addError("Object literal has " + std::to_string(objLit->fields.size()) + 
                                " fields, but type '" + typeName + 
                                "' requires " + std::to_string(expectedFieldCount) + " fields", stmt);
                        return;
                    }
                    
                    // For now, just accept int32/int64 literals for all fields
                    // More detailed type checking can be added later
                    for (const auto& litField : objLit->fields) {
                        Type* fieldValueType = inferType(litField.value.get());
                        if (fieldValueType->getKind() == TypeKind::ERROR) {
                            return;  // Error already reported
                        }
                        
                        // Allow integer literals
                        std::string fieldTypeStr = fieldValueType->toString();
                        if (fieldTypeStr != "int32" && fieldTypeStr != "int64" && 
                            fieldTypeStr != "tbb8" && fieldTypeStr != "tbb16" && 
                            fieldTypeStr != "tbb32" && fieldTypeStr != "tbb64") {
                            addError("Field value in '" + typeName + "' object literal must be an integer, got '" + 
                                    fieldTypeStr + "'", litField.value.get());
                            return;
                        }
                    }
                    
                    // Mark the object literal with the type name for IR generation
                    objLit->type_name = typeName;
                    
                    // Validation passed
                    objectLiteralStructAssignment = true;
                }
            }
        }
        
        // Check if initializer type is assignable to declared type
        // Skip this check if we handled TBB, ERR, balanced, numeric exotic, standard int, high-precision literal, array literal int/object, optional int literal, or object literal struct coercion above
        if (!tbbLiteralAssignment && !errLiteralAssignment && !balancedLiteralAssignment && !numericExoticLiteralAssignment && !standardIntLiteralAssignment && !highPrecisionLiteralAssignment && !arrayLiteralIntCoercion && !arrayLiteralObjectCoercion && !optionalIntLiteralCoercion && !objectLiteralStructAssignment) {
            // Special case: Allow T to be initialized to T? (wrapping)
            bool optionalWrapping = false;
            if (declaredType->getKind() == TypeKind::OPTIONAL) {
                const OptionalType* declaredOptional = static_cast<const OptionalType*>(declaredType);
                if (initType->isAssignableTo(declaredOptional->getWrappedType())) {
                    optionalWrapping = true;
                }
            }
            
            // BUG-003 FIX: Implicit Result<T> → T unwrapping is FORBIDDEN.
            // Every function with a body returns Result<T>. The caller must
            // explicitly handle the result — either by:
            //   1. Declaring the variable as Result<T> and checking .is_error
            //   2. Using raw(expr) to explicitly assert "I know this won't fail"
            // Silent unwrapping violates Aria's safety hierarchy and hides
            // error paths from auditors, lawyers, and the type system.
            if (initType->getKind() == TypeKind::RESULT) {
                const ResultType* resultType = static_cast<const ResultType*>(initType);
                Type* innerType = resultType->getValueType();
                if (innerType->isAssignableTo(declaredType)) {
                    addError("Cannot silently unwrap Result<" + declaredType->toString() + "> "
                            "into '" + stmt->varName + "' of type '" + declaredType->toString() + "'. "
                            "Declare as Result<" + declaredType->toString() + "> and check .is_error, "
                            "or use 'raw expr' to explicitly assert the call cannot fail.", stmt);
                    return;
                }
            }
            // P1.5: Allow automatic void* ↔ wild T-> conversions at FFI boundaries
            bool ffiPointerConversion = canConvertFFIPointer(initType, declaredType);
            
            if (!initType->isAssignableTo(declaredType) && !canCoerce(initType, declaredType) && 
                !optionalWrapping && !ffiPointerConversion) {
                addError("Cannot initialize variable '" + stmt->varName + 
                        "' of type '" + declaredType->toString() + 
                        "' with value of type '" + initType->toString() + "'", stmt);
                return;
            }
        }
    }
    
    // Phase 2.2: Evaluate const expressions at compile time
    ComptimeValue* evaluatedConstValue = nullptr;
    if (stmt->isConst && stmt->initializer) {
        ComptimeValue constValue = constEvaluator->evaluate(stmt->initializer.get());
        // Store evaluated value on heap so it persists
        evaluatedConstValue = new ComptimeValue(constValue);
    }
    
    // Define symbol in symbol table
    symbolTable->defineSymbol(stmt->varName, 
                             stmt->isConst ? SymbolKind::CONSTANT : SymbolKind::VARIABLE,
                             declaredType, 
                             stmt->line, 
                             stmt->column);
    
    // Set fixed flag if this is a fixed variable
    if (stmt->isFixed) {
        Symbol* sym = symbolTable->resolveSymbol(stmt->varName);
        if (sym) {
            sym->isFixed = true;
        }
    }
    
    // Set borrow flags if this is a borrow variable ($$i/$$m)
    if (stmt->isBorrowImm || stmt->isBorrowMut) {
        Symbol* sym = symbolTable->resolveSymbol(stmt->varName);
        if (sym) {
            sym->isBorrowImm = stmt->isBorrowImm;
            sym->isBorrowMut = stmt->isBorrowMut;
        }
    }
    
    // If we evaluated a const value, store it in the symbol
    if (evaluatedConstValue) {
        Symbol* sym = symbolTable->resolveSymbol(stmt->varName);
        if (sym) {
            sym->setComptimeValue(evaluatedConstValue);
        }
    }
    
    // Phase 1.5: Track wild Result variables
    // Wild Results can modify .is_error field (allows error state corruption)
    // Non-wild Results have immutable .is_error (errors must propagate)
    if (declaredType->getKind() == TypeKind::RESULT && stmt->isWild) {
        wildResults.insert(stmt->varName);
    }
    
    // v0.2.41: Validate limit<> constraints
    if (!stmt->limitRulesName.empty()) {
        auto it = rulesTable.find(stmt->limitRulesName);
        if (it == rulesTable.end()) {
            addError("Unknown rules '" + stmt->limitRulesName + "' in limit<>. "
                     "Declare Rules:" + stmt->limitRulesName + " before using it.", stmt);
            return;
        }
        
        RulesDeclStmt* rulesDecl = it->second;
        
        // v0.2.42: Type parameter enforcement
        // v0.2.44: Also matches array types (T[] matches T[N]) and pointer types (T@ matches T@)
        if (!rulesDecl->typeParams.empty()) {
            // Rules has type params — variable type must match one of them
            std::string varTypeName = declaredType->toString();
            bool typeMatch = false;
            for (const auto& tp : rulesDecl->typeParams) {
                if (tp == varTypeName) {
                    typeMatch = true;
                    break;
                }
                // v0.2.44: Array type matching — "int8[]" matches "int8[N]" for any N
                if (tp.length() > 2 && tp.substr(tp.length()-2) == "[]") {
                    std::string baseType = tp.substr(0, tp.length()-2);
                    // varTypeName should be "baseType[N]" for some N
                    if (varTypeName.length() > baseType.length() + 2 &&
                        varTypeName.substr(0, baseType.length()) == baseType &&
                        varTypeName[baseType.length()] == '[' &&
                        varTypeName.back() == ']') {
                        typeMatch = true;
                        break;
                    }
                }
            }
            if (!typeMatch) {
                std::string allowed;
                for (size_t i = 0; i < rulesDecl->typeParams.size(); ++i) {
                    if (i > 0) allowed += ", ";
                    allowed += rulesDecl->typeParams[i];
                }
                addError("limit<" + stmt->limitRulesName + "> can only be applied to " + 
                         allowed + ", got '" + varTypeName + "'", stmt);
                return;
            }
        } else {
            // No type params (backward compat) — only numeric types allowed
            if (!isNumericType(declaredType)) {
                addError("limit<> constraints can only be applied to numeric types, not '" + 
                         declaredType->toString() + "'", stmt);
                return;
            }
        }
        
        // Track this variable as limited for assignment checking
        limitedVariables[stmt->varName] = stmt->limitRulesName;
        
        // Compile-time check: if initializer is a literal, evaluate rules now
        if (stmt->initializer && stmt->initializer->type == ASTNode::NodeType::LITERAL) {
            LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
            if (std::holds_alternative<int64_t>(literal->value)) {
                int64_t value = std::get<int64_t>(literal->value);
                validateLimitRules(stmt->limitRulesName, value, stmt);
            } else if (std::holds_alternative<double>(literal->value)) {
                double value = std::get<double>(literal->value);
                validateLimitRulesFloat(stmt->limitRulesName, value, stmt);
            }
        }
        // Compile-time check: negative literal (unary minus on literal)
        else if (stmt->initializer && stmt->initializer->type == ASTNode::NodeType::UNARY_OP) {
            UnaryExpr* unary = static_cast<UnaryExpr*>(stmt->initializer.get());
            if (unary->op.type == frontend::TokenType::TOKEN_MINUS &&
                unary->operand->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* literal = static_cast<LiteralExpr*>(unary->operand.get());
                if (std::holds_alternative<int64_t>(literal->value)) {
                    int64_t value = -std::get<int64_t>(literal->value);
                    validateLimitRules(stmt->limitRulesName, value, stmt);
                } else if (std::holds_alternative<double>(literal->value)) {
                    double value = -std::get<double>(literal->value);
                    validateLimitRulesFloat(stmt->limitRulesName, value, stmt);
                }
            }
        }
        // Dynamic value: runtime checks will be inserted by IR generator
    }
}

// ============================================================================
// P1-4: Design by Contract - Contract Validation
// ============================================================================

void TypeChecker::validateContracts(const std::vector<ASTNodePtr>& contracts,
                                   bool isPostcondition,
                                   Type* valueType,
                                   ASTNode* stmt) {
    // If no contracts, nothing to validate
    if (contracts.empty()) {
        return;
    }
    
    // For postconditions, temporarily define 'result' symbol
    // This allows postconditions to reference the return value
    Symbol* resultSymbol = nullptr;
    if (isPostcondition && valueType) {
        // Check if 'result' is already defined (would be a parameter name conflict)
        Symbol* existing = symbolTable->lookupSymbol("result");
        if (existing) {
            addError("Cannot use 'result' as parameter name - reserved for postconditions", stmt);
        } else {
            resultSymbol = symbolTable->defineSymbol("result", SymbolKind::VARIABLE,
                                                    valueType, stmt->line, stmt->column);
        }
    }
    
    // Validate each contract clause
    for (size_t i = 0; i < contracts.size(); ++i) {
        ASTNodePtr contract = contracts[i];
        if (!contract) {
            addError("Contract clause is null", stmt);
            continue;
        }
        
        // Infer the type of the contract expression
        Type* contractType = inferType(contract.get());
        
        if (!contractType) {
            std::string clauseType = isPostcondition ? "ensures" : "requires";
            addError("Failed to infer type for " + clauseType + " clause #" + 
                    std::to_string(i + 1), stmt);
            continue;
        }
        
        // Contract expressions must evaluate to boolean
        // bool is a primitive type with name "bool"
        bool isBool = false;
        if (contractType->getKind() == TypeKind::PRIMITIVE) {
            PrimitiveType* primType = static_cast<PrimitiveType*>(contractType);
            isBool = (primType->getName() == "bool");
        }
        
        if (!isBool) {
            std::string clauseType = isPostcondition ? "ensures" : "requires";
            addError("Contract " + clauseType + " clause #" + std::to_string(i + 1) + 
                    " must be boolean expression, got " + contractType->toString(), stmt);
        }
        
        // Side-effect check: contracts must be pure (no function calls)
        // Contract expressions describe predicates over parameters and result.
        // Calling arbitrary functions could mutate state, violating the contract
        // semantics. Reject call expressions in contract clauses.
        if (contract->type == ASTNode::NodeType::CALL) {
            std::string clauseType = isPostcondition ? "ensures" : "requires";
            addError("Contract " + clauseType + " clause #" + std::to_string(i + 1) +
                    " must be a pure expression (function calls are not allowed)", stmt);
        }
    }
    
    // Note: resultSymbol will be automatically cleaned up when function scope exits
    // No need to manually remove it
}

// ============================================================================
// Function Declaration Type Checking
// ============================================================================

void TypeChecker::checkFuncDecl(FuncDeclStmt* stmt) {
    // Skip detailed type checking for generic functions
    // Their bodies will be checked during monomorphization when types are concrete
    if (!stmt->genericParams.empty()) {
        // Register the generic function in the symbol table so it can be found during calls
        // Create a placeholder function type (will be specialized during monomorphization)
        std::vector<Type*> placeholderParams;
        for (const auto& param : stmt->parameters) {
            placeholderParams.push_back(typeSystem->getUnknownType());
        }
        Type* placeholderReturn = typeSystem->getUnknownType();
        Type* funcType = new FunctionType(placeholderParams, placeholderReturn, stmt->isAsync, stmt->isVariadic);
        
        Symbol* funcSymbol = symbolTable->defineSymbol(stmt->funcName, SymbolKind::FUNCTION,
                                                        funcType, stmt->line, stmt->column);
        if (funcSymbol) {
            funcSymbol->setFuncDecl(stmt);  // Store AST for generic resolution
        }
        
        // Don't check the body - it will be checked during monomorphization
        return;
    }
    
    // Register comptime functions with the ConstEvaluator for CTFE
    if (stmt->isComptime) {
        constEvaluator->registerFunction(stmt->funcName, stmt);
    }
    
    // Resolve return type from the type node
    Type* valueType = resolveTypeNode(stmt->returnType.get());
    
    if (!valueType) {
        if (stmt->returnType) {
            addError("Unknown return type: '" + stmt->returnType->toString() + "'", stmt);
        } else {
            addError("Missing return type", stmt);
        }
        return;
    }

    // Validate main function signature
    // main must return int32 to be compatible with C runtime expectations
    if (stmt->funcName == "main") {
        std::string returnTypeName = valueType->toString();
        if (returnTypeName != "int32") {
            addError("Function 'main' must return 'int32', but returns '" +
                     returnTypeName + "'", stmt);
            // Continue checking to report other errors, but main is invalid
        }

        // main should take no parameters (for now - could support int argc, char** argv later)
        if (!stmt->parameters.empty()) {
            addError("Function 'main' should take no parameters", stmt);
        }
    }

    // Validate failsafe function signature
    // failsafe must return int32 and take exactly one tbb32 parameter
    if (stmt->funcName == "failsafe") {
        std::string returnTypeName = valueType->toString();
        if (returnTypeName != "int32") {
            addError("Function 'failsafe' must return 'int32', but returns '" +
                     returnTypeName + "'. Use 'exit' to terminate, not 'pass'.", stmt);
        }

        if (stmt->parameters.size() != 1) {
            addError("Function 'failsafe' must take exactly one parameter: tbb32:err_code", stmt);
        } else {
            // Check parameter type is tbb32
            auto& param = stmt->parameters[0];
            if (param->type == ASTNode::NodeType::PARAMETER) {
                ParameterNode* pn = static_cast<ParameterNode*>(param.get());
                if (pn->typeNode) {
                    std::string paramTypeName = pn->typeNode->toString();
                    if (paramTypeName != "tbb32") {
                        addError("Function 'failsafe' parameter must be tbb32, got '" +
                                 paramTypeName + "'. tbb32 allows ERR signal for unknown errors.", stmt);
                    }
                }
            }
        }
    }

    // ARIA-026 FIX: FFI Safety - Ban string return types from extern functions
    if (!stmt->body && valueType->toString() == "str") {  // No body = extern function
        addError("FFI Safety Violation: Cannot return 'str' from extern function '" + 
                 stmt->funcName + "'. Aria strings are fat pointers {data, len} incompatible " +
                 "with C char*. Use 'wild int8*' for C strings and convert with string_from_cstr().", 
                 stmt);
    }

    // Return type handling:
    // - Regular Aria functions return Result<T> for error handling
    // - Extern functions (no body) return raw T for C ABI compatibility
    
    // DESIGN ENFORCEMENT: void type only allowed in extern blocks
    // Aria uses NIL for no-value returns to force unfamiliarity and prevent C assumptions
    Type* voidType = typeSystem->getPrimitiveType("void");
    if (!stmt->body) {
        // Extern function: void is allowed (FFI boundary)
    } else if (valueType->equals(voidType)) {
        // Non-extern function: void is forbidden, use NIL instead
        addError("void return type only allowed in extern blocks. Use NIL for no-value returns in Aria code.", stmt);
        return;
    }
    
    Type* actualReturnType;
    if (!stmt->body) {
        // Extern function: return raw type (no Result wrapping)
        actualReturnType = valueType;
    } else {
        // Regular function: return Result<T>
        actualReturnType = new ResultType(valueType);
    }

    // Set current function return types for pass/fail/return statement checking
    Type* previousReturnType = currentFunctionReturnType;
    Type* previousValueType = currentFunctionValueType;
    std::string previousFunctionName = currentFunctionName;
    currentFunctionReturnType = actualReturnType;
    currentFunctionValueType = valueType;
    currentFunctionName = stmt->funcName;
    
    // PRE-REGISTER FUNCTION SYMBOL FOR SELF-RECURSION SUPPORT
    // Build a forward-reference symbol BEFORE entering the function scope so the
    // function name is visible inside the body.  This allows recursive calls like
    //   func:fib = int32(int32:n) { ... pass(fib(n-1i32)); ... }
    // to resolve correctly during type-checking.
    {
        std::vector<Type*> earlyParamTypes;
        for (const auto& param : stmt->parameters) {
            if (param->type == ASTNode::NodeType::PARAMETER) {
                ParameterNode* paramNode = static_cast<ParameterNode*>(param.get());
                Type* pt = resolveTypeNode(paramNode->typeNode.get());
                earlyParamTypes.push_back((pt && pt->getKind() != TypeKind::ERROR)
                    ? pt : typeSystem->getPrimitiveType("void"));
            }
        }
        Type* earlyFuncType = new FunctionType(earlyParamTypes, actualReturnType,
                                               stmt->isAsync, stmt->isVariadic);
        Symbol* earlySym = symbolTable->defineSymbol(stmt->funcName, SymbolKind::FUNCTION,
                                                      earlyFuncType,
                                                      stmt->line, stmt->column);
        if (earlySym) {
            earlySym->setFuncDecl(stmt);
        }
    }

    // CRITICAL FIX: Enter function scope BEFORE defining parameters
    // This prevents parameter name collisions between different functions
    // Without this, all parameters are registered in global scope and collide!
    symbolTable->enterScope(ScopeKind::FUNCTION, stmt->funcName);
    
    // Check parameters
    for (const auto& param : stmt->parameters) {
        if (param->type == ASTNode::NodeType::PARAMETER) {
            ParameterNode* paramNode = static_cast<ParameterNode*>(param.get());
            
            // Resolve parameter type from the type node
            Type* paramType = resolveTypeNode(paramNode->typeNode.get());
            
            if (!paramType) {
                if (paramNode->typeNode) {
                    addError("Unknown parameter type: '" + paramNode->typeNode->toString() + "'", param.get());
                } else {
                    addError("Missing parameter type", param.get());
                }
                continue;
            }
            
            // ARIA-026 FIX: FFI Safety - Ban strings in extern functions
            // String type is a fat pointer {data, len} struct which is incompatible with C ABI char*.
            // Passing string to extern causes parameter shift: C sees 'data' as arg1, 'len' as arg2.
            // This corrupts actuator force limits and sensor readings in robotic control.
            if (!stmt->body && paramType->toString() == "str") {  // No body = extern function
                addError("FFI Safety Violation: Cannot pass 'str' type to extern function '" + 
                         stmt->funcName + "'. Aria strings are fat pointers {data, len} incompatible " +
                         "with C char*. Use 'wild int8*' for C strings and convert with string_to_cstr().", 
                         param.get());
            }
            
            // Define parameter in symbol table (will be visible in function body)
            // FUNCTION_TYPE params (function pointer args) use UNKNOWN so call sites don't error.
            if (paramNode->typeNode &&
                paramNode->typeNode->type == ASTNode::NodeType::FUNCTION_TYPE) {
                symbolTable->defineSymbol(paramNode->paramName, SymbolKind::VARIABLE,
                                         typeSystem->getUnknownType(), param->line, param->column);
                continue;
            }
            symbolTable->defineSymbol(paramNode->paramName, SymbolKind::VARIABLE,
                                     paramType, param->line, param->column);
        }
    }
    
    // P1-4: Validate contract clauses (Design by Contract)
    // Check preconditions (requires) - can reference parameters only
    validateContracts(stmt->preconditions, false, valueType, stmt);
    
    // Check postconditions (ensures) - can reference parameters and 'result'
    validateContracts(stmt->postconditions, true, valueType, stmt);
    
    // Check function body (this is where generic calls get analyzed!)
    if (stmt->body) {
        checkStatement(stmt->body.get());
    }
    
    // Exit function scope AFTER body is checked
    symbolTable->exitScope();
    
    // Restore previous function return types
    currentFunctionReturnType = previousReturnType;
    currentFunctionValueType = previousValueType;
    currentFunctionName = previousFunctionName;
    
    // Note: The function symbol was pre-registered before the scope entry (above)
    // to support self-recursion.  No further defineSymbol() call needed here.
}

// ============================================================================
// Comptime Expression and Block Type Checking
// ============================================================================

Type* TypeChecker::inferComptimeExpr(ComptimeExpr* expr) {
    if (!expr || !expr->expr) {
        addError("Invalid comptime expression", expr);
        return typeSystem->getErrorType();
    }
    
    // First, type-check the inner expression normally
    Type* innerType = inferType(expr->expr.get());
    if (!innerType || innerType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Evaluate the expression at compile time
    ComptimeValue result = constEvaluator->evaluate(expr->expr.get());
    
    // Check for evaluation errors
    if (constEvaluator->hasErrors()) {
        for (const auto& err : constEvaluator->getErrors()) {
            addError("comptime evaluation failed: " + err, expr);
        }
        constEvaluator->clearErrors();
        return typeSystem->getErrorType();
    }
    
    // Store the result on the AST node for the IR generator
    expr->evaluated = true;
    
    // Map the ComptimeValue kind back to a Type* and store result
    switch (result.getKind()) {
        case ComptimeValue::Kind::INTEGER:
        case ComptimeValue::Kind::UNSIGNED:
        case ComptimeValue::Kind::TBB: {
            expr->intResult = result.getInt();
            const std::string& typeName = result.getTypeName();
            expr->resultTypeName = typeName.empty() ? "int64" : typeName;
            Type* t = typeSystem->getPrimitiveType(expr->resultTypeName);
            return t ? t : typeSystem->getPrimitiveType("int64");
        }
        case ComptimeValue::Kind::FLOAT: {
            expr->floatResult = result.getFloat();
            const std::string& typeName = result.getTypeName();
            expr->resultTypeName = typeName.empty() ? "flt64" : typeName;
            Type* t = typeSystem->getPrimitiveType(expr->resultTypeName);
            return t ? t : typeSystem->getPrimitiveType("flt64");
        }
        case ComptimeValue::Kind::BOOL:
            expr->boolResult = result.getBool();
            expr->resultTypeName = "bool";
            return typeSystem->getPrimitiveType("bool");
        case ComptimeValue::Kind::STRING:
            expr->stringResult = result.getString();
            expr->resultTypeName = "str";
            return typeSystem->getPrimitiveType("str");
        default:
            // For complex types (arrays, structs, etc.), use the inner expression's type
            return innerType;
    }
}

void TypeChecker::checkComptimeBlock(ComptimeBlockStmt* stmt) {
    if (!stmt || !stmt->body) {
        addError("Invalid comptime block", stmt);
        return;
    }
    
    // Evaluate the block at compile time
    constEvaluator->evaluate(stmt->body.get());
    
    // Propagate any evaluation errors
    if (constEvaluator->hasErrors()) {
        for (const auto& err : constEvaluator->getErrors()) {
            addError("comptime block evaluation failed: " + err, stmt);
        }
        constEvaluator->clearErrors();
    }
}

// ============================================================================
// Struct Declaration Type Checking
// ============================================================================

void TypeChecker::checkStructDecl(StructDeclStmt* stmt) {
    // Session 13: Check if this is a generic struct
    if (!stmt->genericParams.empty()) {
        // Generic struct - register for later monomorphization
        // Don't create the type yet - it will be created when instantiated
        auto it = genericStructRegistry.find(stmt->structName);
        if (it != genericStructRegistry.end()) {
            // Already pre-registered during preRegisterFunctions — skip silently
            return;
        }
        genericStructRegistry[stmt->structName] = stmt;
        return;  // Don't register with TypeSystem yet
    }
    
    // Non-generic struct - check if already defined.
    // The pre-registration pass calls checkStructDecl early so that struct
    // types are available during function signature resolution.  When the
    // main pass revisits the same declaration, skip silently rather than
    // emitting a spurious "already defined" error.
    Type* existingStruct = typeSystem->getStructType(stmt->structName);
    if (existingStruct) {
        // Silently skip: struct was pre-registered by preRegisterFunctions.
        // A genuine duplicate definition in user code is caught by the
        // pre-pass itself (it only registers if !getStructType()), so we
        // will never reach this point for true duplicates.
        return;
    }
    
    // Note: We don't check primitiveCache because getPrimitiveType auto-creates types
    // This is fine - struct names should just avoid conflicting with known primitives
    
    // Extract and validate field information
    std::vector<StructType::Field> fields;
    for (const auto& field : stmt->fields) {
        if (field->type == ASTNode::NodeType::VAR_DECL) {
            VarDeclStmt* fieldDecl = static_cast<VarDeclStmt*>(field.get());
            
            // v0.2.35: Borrow qualifiers not allowed on struct fields
            if (fieldDecl->isBorrowImm || fieldDecl->isBorrowMut) {
                addError("Borrow qualifiers ($$i/$$m) cannot be used on struct fields — "
                         "structs cannot hold borrowed references (v1 restriction)", field.get());
                continue;
            }
            
            // Extract type name from typeNode (parser stores it there, not in typeName)
            std::string fieldTypeName;
            Type* fieldType = nullptr;
            
            if (fieldDecl->typeNode) {
                // Use resolveTypeNode to handle all type kinds (simple, array, pointer, etc.)
                fieldType = resolveTypeNode(fieldDecl->typeNode.get());
                
                if (!fieldType || fieldType->getKind() == TypeKind::ERROR) {
                    addError("Invalid type for field '" + fieldDecl->varName + 
                            "' in struct '" + stmt->structName + "'", field.get());
                    continue;
                }
                
            } else if (!fieldDecl->typeName.empty()) {
                // Fallback to legacy typeName field for backwards compatibility
                fieldTypeName = fieldDecl->typeName;
                
                // Look up field type (check struct types first to avoid auto-creating primitives)
                fieldType = typeSystem->getStructType(fieldTypeName);
                if (!fieldType) {
                    fieldType = typeSystem->getPrimitiveType(fieldTypeName);
                }
                
                if (!fieldType || fieldType->getKind() == TypeKind::ERROR) {
                    addError("Unknown field type '" + fieldTypeName + 
                            "' in struct '" + stmt->structName + "'", field.get());
                    continue;
                }
                
            } else {
                addError("Missing type information for field '" + fieldDecl->varName + 
                        "' in struct '" + stmt->structName + "'", field.get());
                continue;
            }
            
            // Add field to list (offset 0 for now, isPublic false for now)
            fields.push_back(StructType::Field(fieldDecl->varName, fieldType, 0, false));
        }
    }
    
    // Register struct type in type system with field information
    typeSystem->createStructType(stmt->structName, fields, 0, 0, false);
    
    // v0.8.3: Expand derive attributes if present
    if (!stmt->attributes.empty()) {
        expandDeriveAttributes(stmt);
    }
}

void TypeChecker::checkEnumDecl(EnumDeclStmt* stmt) {
    // v0.2.39: Create proper EnumType for type-safe enum variables
    
    // Check if enum name already exists as a type
    if (typeSystem->getStructType(stmt->enumName) || typeSystem->getEnumType(stmt->enumName)) {
        addError("Type '" + stmt->enumName + "' is already defined", stmt);
        return;
    }
    
    // Create the EnumType in the type system
    EnumType* enumType = typeSystem->createEnumType(stmt->enumName, stmt->variants);
    
    // Register each enum variant as a typed constant in the symbol table
    for (const auto& [variantName, variantValue] : stmt->variants) {
        std::string fullName = stmt->enumName + "." + variantName;
        
        if (symbolTable->isDefined(fullName)) {
            addError("Enum variant '" + variantName + "' is already defined in enum '" + 
                    stmt->enumName + "'", stmt);
            continue;
        }
        
        // Register variant with the enum type (not int64 — enables type checking)
        symbolTable->defineSymbol(fullName, SymbolKind::VARIABLE, enumType);
    }
    
    // Also register the enum name itself so it can be used as a type annotation
    // (resolveTypeNode will look it up via typeSystem->getEnumType())
}

// ============================================================================
// v0.2.41: Rules Declaration — Refinement Types
// ============================================================================

// v0.2.43: Recursively validate $.field references in rule conditions
void TypeChecker::validateRulesDollarFields(ASTNode* node, const std::string& typeName,
                                             StructType* structType, ASTNode* errorNode) {
    if (!node) return;
    
    if (node->type == ASTNode::NodeType::MEMBER_ACCESS) {
        auto* member = static_cast<MemberAccessExpr*>(node);
        // Check if the object is the $ placeholder
        if (member->object->type == ASTNode::NodeType::IDENTIFIER) {
            auto* ident = static_cast<IdentifierExpr*>(member->object.get());
            if (ident->name == "$") {
                // Validate the field against the type parameter
                if (typeName == "string") {
                    // String built-in properties and methods
                    static const std::unordered_set<std::string> validStringMembers = {
                        "length", "contains", "startsWith", "endsWith"
                    };
                    if (validStringMembers.find(member->member) == validStringMembers.end()) {
                        addError("String type has no property '" + member->member + 
                                 "' in Rules body. Valid: $.length, $.contains(), $.startsWith(), $.endsWith()", errorNode);
                    }
                }
                // v0.2.44: Array type — accept .length
                else if (typeName.length() > 2 && typeName.substr(typeName.length()-2) == "[]") {
                    if (member->member != "length") {
                        addError("Array type has no property '" + member->member + 
                                 "' in Rules body. Valid: $.length", errorNode);
                    }
                }
                else if (structType) {
                    // Struct type — check field exists
                    bool found = false;
                    const auto& fields = structType->getFields();
                    for (const auto& field : fields) {
                        if (field.name == member->member) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        std::string available;
                        for (size_t i = 0; i < fields.size(); ++i) {
                            if (i > 0) available += ", ";
                            available += fields[i].name;
                        }
                        addError("Type '" + typeName + "' has no field '" + member->member +
                                 "' in Rules body. Available fields: " + available, errorNode);
                    }
                }
            }
        }
        // v0.2.44: Also handle $.length on $[idx] (e.g., member access on indexed expression)
        // where the object is an IndexExpr
        if (member->object->type == ASTNode::NodeType::INDEX) {
            validateRulesDollarFields(member->object.get(), typeName, structType, errorNode);
        }
    }
    
    // v0.2.44: Recurse into index expressions (for $[idx] in conditions)
    if (node->type == ASTNode::NodeType::INDEX) {
        auto* indexExpr = static_cast<IndexExpr*>(node);
        validateRulesDollarFields(indexExpr->array.get(), typeName, structType, errorNode);
    }
    
    // Recurse into binary expressions
    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* binary = static_cast<BinaryExpr*>(node);
        validateRulesDollarFields(binary->left.get(), typeName, structType, errorNode);
        validateRulesDollarFields(binary->right.get(), typeName, structType, errorNode);
    }
    
    // Recurse into call expressions (for $.contains() etc.)
    if (node->type == ASTNode::NodeType::CALL) {
        auto* call = static_cast<CallExpr*>(node);
        validateRulesDollarFields(call->callee.get(), typeName, structType, errorNode);
        for (const auto& arg : call->arguments) {
            validateRulesDollarFields(arg.get(), typeName, structType, errorNode);
        }
    }
}

void TypeChecker::checkRulesDecl(RulesDeclStmt* stmt) {
    // Check for duplicate rules name
    if (rulesTable.find(stmt->rulesName) != rulesTable.end()) {
        addError("Rules '" + stmt->rulesName + "' is already defined", stmt);
        return;
    }
    
    // v0.2.42: Validate type parameters are known types
    // v0.2.43: Also accept struct/Type names for $.field member access
    // v0.2.44: Also accept array types (T[]) and pointer types (T@)
    for (const auto& typeParam : stmt->typeParams) {
        std::string baseType = typeParam;
        // v0.2.44: Strip array suffix [] or pointer suffix @ to validate the base type
        if (baseType.length() > 2 && baseType.substr(baseType.length()-2) == "[]") {
            baseType = baseType.substr(0, baseType.length()-2);
        } else if (baseType.length() > 1 && baseType.back() == '@') {
            baseType = baseType.substr(0, baseType.length()-1);
        }
        if (!isTypeKeyword(baseType) && !typeSystem->getStructType(baseType)) {
            std::string suggestion = findSimilarType(baseType);
            std::string msg = "Unknown type '" + typeParam + "' in Rules<> type parameter";
            if (!suggestion.empty()) {
                msg += ". Did you mean '" + suggestion + "'?";
            }
            addError(msg, stmt);
            return;
        }
    }
    
    // Validate cascaded rule references exist
    for (const auto& cascadedName : stmt->cascadedRules) {
        if (rulesTable.find(cascadedName) == rulesTable.end()) {
            addError("Cascaded rules '" + cascadedName + "' referenced in Rules:" + 
                     stmt->rulesName + " has not been declared yet", stmt);
            return;
        }
    }
    
    // v0.2.43: Validate $.field member access against the type parameter
    if (!stmt->typeParams.empty()) {
        const std::string& typeName = stmt->typeParams[0];
        StructType* structType = nullptr;
        Type* resolvedType = typeSystem->getStructType(typeName);
        if (resolvedType && resolvedType->getKind() == TypeKind::STRUCT) {
            structType = static_cast<StructType*>(resolvedType);
        }
        
        // Walk each condition looking for MemberAccessExpr on $
        for (const auto& cond : stmt->conditions) {
            validateRulesDollarFields(cond.get(), typeName, structType, stmt);
        }
    }
    
    // Register in rules table
    rulesTable[stmt->rulesName] = stmt;
}

// Compile-time evaluation of limit rules against an integer literal value
void TypeChecker::validateLimitRules(const std::string& rulesName, int64_t value, ASTNode* stmt) {
    auto it = rulesTable.find(rulesName);
    if (it == rulesTable.end()) return;
    
    RulesDeclStmt* rules = it->second;
    
    // Check cascaded rules first (recursively)
    for (const auto& cascadedName : rules->cascadedRules) {
        validateLimitRules(cascadedName, value, stmt);
    }
    
    // Evaluate each condition by substituting $ with the value
    for (size_t i = 0; i < rules->conditions.size(); ++i) {
        auto& condition = rules->conditions[i];
        if (!evaluateRuleConditionInt(condition.get(), value)) {
            addError("limit<" + rulesName + "> violation: value " + std::to_string(value) +
                     " fails rule condition '" + condition->toString() + "'", stmt);
        }
    }
}

// Compile-time evaluation of limit rules against a float literal value
void TypeChecker::validateLimitRulesFloat(const std::string& rulesName, double value, ASTNode* stmt) {
    auto it = rulesTable.find(rulesName);
    if (it == rulesTable.end()) return;
    
    RulesDeclStmt* rules = it->second;
    
    // Check cascaded rules first (recursively)
    for (const auto& cascadedName : rules->cascadedRules) {
        validateLimitRulesFloat(cascadedName, value, stmt);
    }
    
    // Evaluate each condition by substituting $ with the value
    for (size_t i = 0; i < rules->conditions.size(); ++i) {
        auto& condition = rules->conditions[i];
        if (!evaluateRuleConditionFloat(condition.get(), value)) {
            addError("limit<" + rulesName + "> violation: value " + std::to_string(value) +
                     " fails rule condition '" + condition->toString() + "'", stmt);
        }
    }
}

// Evaluate a single rule condition AST node with integer $ substitution
bool TypeChecker::evaluateRuleConditionInt(ASTNode* node, int64_t dollarValue) {
    if (!node) return true;
    
    // Identifier "$" → return the dollar value
    if (node->type == ASTNode::NodeType::IDENTIFIER) {
        auto* ident = static_cast<IdentifierExpr*>(node);
        if (ident->name == "$") return true; // Shouldn't happen at leaf in a binary expr
    }
    
    // Binary expression: evaluate both sides
    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* binary = static_cast<BinaryExpr*>(node);
        int64_t left = evalRuleOperandInt(binary->left.get(), dollarValue);
        int64_t right = evalRuleOperandInt(binary->right.get(), dollarValue);
        
        switch (binary->op.type) {
            case frontend::TokenType::TOKEN_LESS:          return left < right;
            case frontend::TokenType::TOKEN_LESS_EQUAL:    return left <= right;
            case frontend::TokenType::TOKEN_GREATER:       return left > right;
            case frontend::TokenType::TOKEN_GREATER_EQUAL: return left >= right;
            case frontend::TokenType::TOKEN_EQUAL_EQUAL:   return left == right;
            case frontend::TokenType::TOKEN_BANG_EQUAL:    return left != right;
            default:
                // For non-comparison operators (%, +, etc.), this is a sub-expression
                // that should have been evaluated in evalRuleOperandInt
                return true;
        }
    }
    
    return true; // Unknown node type — pass (runtime will catch it)
}

// Evaluate a rule operand (part of a condition) to an integer value
int64_t TypeChecker::evalRuleOperandInt(ASTNode* node, int64_t dollarValue) {
    if (!node) return 0;
    
    // $ placeholder
    if (node->type == ASTNode::NodeType::IDENTIFIER) {
        auto* ident = static_cast<IdentifierExpr*>(node);
        if (ident->name == "$") return dollarValue;
    }
    
    // Integer literal
    if (node->type == ASTNode::NodeType::LITERAL) {
        auto* literal = static_cast<LiteralExpr*>(node);
        if (std::holds_alternative<int64_t>(literal->value)) {
            return std::get<int64_t>(literal->value);
        }
        if (std::holds_alternative<double>(literal->value)) {
            return static_cast<int64_t>(std::get<double>(literal->value));
        }
    }
    
    // Unary minus
    if (node->type == ASTNode::NodeType::UNARY_OP) {
        auto* unary = static_cast<UnaryExpr*>(node);
        if (unary->op.type == frontend::TokenType::TOKEN_MINUS) {
            return -evalRuleOperandInt(unary->operand.get(), dollarValue);
        }
    }
    
    // Binary sub-expression (e.g., $ % 2 in "$ % 2 == 0")
    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* binary = static_cast<BinaryExpr*>(node);
        int64_t left = evalRuleOperandInt(binary->left.get(), dollarValue);
        int64_t right = evalRuleOperandInt(binary->right.get(), dollarValue);
        
        switch (binary->op.type) {
            case frontend::TokenType::TOKEN_PLUS:    return left + right;
            case frontend::TokenType::TOKEN_MINUS:   return left - right;
            case frontend::TokenType::TOKEN_STAR:    return left * right;
            case frontend::TokenType::TOKEN_SLASH:   return (right != 0) ? left / right : 0;
            case frontend::TokenType::TOKEN_PERCENT: return (right != 0) ? left % right : 0;
            default: return 0;
        }
    }
    
    return 0;
}

// Evaluate a single rule condition AST node with float $ substitution
bool TypeChecker::evaluateRuleConditionFloat(ASTNode* node, double dollarValue) {
    if (!node) return true;
    
    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* binary = static_cast<BinaryExpr*>(node);
        double left = evalRuleOperandFloat(binary->left.get(), dollarValue);
        double right = evalRuleOperandFloat(binary->right.get(), dollarValue);
        
        switch (binary->op.type) {
            case frontend::TokenType::TOKEN_LESS:          return left < right;
            case frontend::TokenType::TOKEN_LESS_EQUAL:    return left <= right;
            case frontend::TokenType::TOKEN_GREATER:       return left > right;
            case frontend::TokenType::TOKEN_GREATER_EQUAL: return left >= right;
            case frontend::TokenType::TOKEN_EQUAL_EQUAL:   return left == right;
            case frontend::TokenType::TOKEN_BANG_EQUAL:    return left != right;
            default: return true;
        }
    }
    
    return true;
}

double TypeChecker::evalRuleOperandFloat(ASTNode* node, double dollarValue) {
    if (!node) return 0.0;
    
    if (node->type == ASTNode::NodeType::IDENTIFIER) {
        auto* ident = static_cast<IdentifierExpr*>(node);
        if (ident->name == "$") return dollarValue;
    }
    
    if (node->type == ASTNode::NodeType::LITERAL) {
        auto* literal = static_cast<LiteralExpr*>(node);
        if (std::holds_alternative<double>(literal->value)) {
            return std::get<double>(literal->value);
        }
        if (std::holds_alternative<int64_t>(literal->value)) {
            return static_cast<double>(std::get<int64_t>(literal->value));
        }
    }
    
    if (node->type == ASTNode::NodeType::UNARY_OP) {
        auto* unary = static_cast<UnaryExpr*>(node);
        if (unary->op.type == frontend::TokenType::TOKEN_MINUS) {
            return -evalRuleOperandFloat(unary->operand.get(), dollarValue);
        }
    }
    
    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* binary = static_cast<BinaryExpr*>(node);
        double left = evalRuleOperandFloat(binary->left.get(), dollarValue);
        double right = evalRuleOperandFloat(binary->right.get(), dollarValue);
        
        switch (binary->op.type) {
            case frontend::TokenType::TOKEN_PLUS:    return left + right;
            case frontend::TokenType::TOKEN_MINUS:   return left - right;
            case frontend::TokenType::TOKEN_STAR:    return left * right;
            case frontend::TokenType::TOKEN_SLASH:   return (right != 0.0) ? left / right : 0.0;
            case frontend::TokenType::TOKEN_PERCENT: return (right != 0.0) ? std::fmod(left, right) : 0.0;
            default: return 0.0;
        }
    }
    
    return 0.0;
}

// ============================================================================
// Type Oriented Programming (TOP) - Type Declaration Desugaring
// ============================================================================

void TypeChecker::checkTypeDecl(TypeDeclStmt* stmt) {
    // Type Oriented Programming: Desugar Type:Name into struct + prefixed methods
    // This provides class-like organization without OOP baggage
    
    // Step 1: Check if Type name already exists
    Type* existingType = typeSystem->getStructType(stmt->typeName);
    bool preRegistered = (existingType != nullptr);
    if (preRegistered) {
        // The struct was pre-registered by the TYPE_DECL pre-pass in preRegisterFunctions
        // so that function signatures could resolve this type during the function pre-pass.
        // We still need to process and register the methods below — skip to Step 3.
        goto register_methods;
    }
    
    {
    // Step 2: Merge struct:internal + struct:interface into single struct
    std::vector<ASTNodePtr> mergedFields;
    
    if (stmt->internalStruct) {
        auto internalStruct = std::static_pointer_cast<StructDeclStmt>(stmt->internalStruct);
        for (const auto& field : internalStruct->fields) {
            mergedFields.push_back(field);
        }
    }
    
    if (stmt->interfaceStruct) {
        auto interfaceStruct = std::static_pointer_cast<StructDeclStmt>(stmt->interfaceStruct);
        for (const auto& field : interfaceStruct->fields) {
            mergedFields.push_back(field);
        }
    }
    
    // Create merged struct declaration
    auto mergedStruct = std::make_shared<StructDeclStmt>(
        stmt->typeName,
        mergedFields,
        stmt->line,
        stmt->column
    );
    
    // Process the merged struct through normal struct checking
    checkStructDecl(mergedStruct.get());
    } // end scope for Step 2 locals
    
register_methods:
    {
    // Step 3: Register Type in symbol table with the actual (now-created) struct type
    Type* structType = typeSystem->getStructType(stmt->typeName);
    symbolTable->defineSymbol(stmt->typeName, SymbolKind::TYPE, structType);
    
    // Step 4: Process func:create as TypeName_create
    if (stmt->createFunc) {
        auto createFunc = std::static_pointer_cast<FuncDeclStmt>(stmt->createFunc);
        std::string prefixedName = stmt->typeName + "_" + createFunc->funcName;
        createFunc->funcName = prefixedName;
        checkFuncDecl(createFunc.get());
    }
    
    // Step 5: Process func:destroy as TypeName_destroy
    if (stmt->destroyFunc) {
        auto destroyFunc = std::static_pointer_cast<FuncDeclStmt>(stmt->destroyFunc);
        std::string prefixedName = stmt->typeName + "_" + destroyFunc->funcName;
        destroyFunc->funcName = prefixedName;
        checkFuncDecl(destroyFunc.get());
    }
    
    // Step 6: Process all methods as TypeName_methodName
    for (const auto& method : stmt->methods) {
        auto methodFunc = std::static_pointer_cast<FuncDeclStmt>(method);
        std::string prefixedName = stmt->typeName + "_" + methodFunc->funcName;
        methodFunc->funcName = prefixedName;
        checkFuncDecl(methodFunc.get());
    }
    
    // Note: struct:type (static members) would be handled similarly to regular structs
    // For now, we skip static member struct processing
    // Type.MEMBER access is already transformed by parser to Type_MEMBER() calls
    } // end register_methods block
}

// ============================================================================
// Trait System Type Checking (WP 005)
// ============================================================================

void TypeChecker::checkTraitDecl(TraitDeclStmt* stmt) {
    // Check if trait name already exists
    if (symbolTable->isDefined(stmt->traitName)) {
        addError("Trait '" + stmt->traitName + "' is already defined", stmt);
        return;
    }

    // Register the trait in the symbol table
    Symbol* traitSymbol = symbolTable->defineSymbol(
        stmt->traitName,
        SymbolKind::TRAIT,
        nullptr  // Traits don't have a direct Type representation yet
    );

    if (traitSymbol) {
        traitSymbol->setTraitDecl(stmt);
    }

    // Validate method signatures (basic validation)
    for (const auto& method : stmt->methods) {
        // Validate return type exists
        if (!method.returnType.empty() && method.returnType != "void") {
            if (!isTypeKeyword(method.returnType) &&
                !typeSystem->getStructType(method.returnType) &&
                !symbolTable->isDefined(method.returnType)) {
                addError("Unknown return type '" + method.returnType +
                        "' in trait method '" + method.name + "'", stmt);
            }
        }
        // Validate parameter types exist (skip 'self' which is patched at impl time)
        for (const auto& param : method.parameters) {
            if (param.paramName == "self") continue;
            if (param.typeNode) {
                // Resolve via the standard type resolution path
                Type* paramType = resolveTypeNode(param.typeNode.get());
                if (!paramType || paramType->getKind() == TypeKind::ERROR) {
                    // resolveTypeNode already reported the error
                }
            }
        }
    }
}

void TypeChecker::checkImplDecl(ImplDeclStmt* stmt) {
    // Look up the trait
    Symbol* traitSymbol = symbolTable->lookupSymbol(stmt->traitName);
    if (!traitSymbol || traitSymbol->kind != SymbolKind::TRAIT) {
        addError("Trait '" + stmt->traitName + "' is not defined", stmt);
        return;
    }

    // Look up the type (struct) being implemented for
    Symbol* typeSymbol = symbolTable->lookupSymbol(stmt->typeName);
    if (!typeSymbol || typeSymbol->kind != SymbolKind::TYPE) {
        // Type might be a primitive type keyword (impl:Numeric:for:int32 etc.)
        // or a struct not yet in the symbol table but known to the type system
        if (!isTypeKeyword(stmt->typeName) &&
            !typeSystem->getPrimitiveType(stmt->typeName) &&
            !typeSystem->getStructType(stmt->typeName)) {
            addError("Type '" + stmt->typeName + "' is not defined", stmt);
            return;
        }
    }

    // Get the trait declaration
    TraitDeclStmt* traitDecl = traitSymbol->getTraitDecl();
    if (!traitDecl) {
        addError("Internal error: Trait '" + stmt->traitName + "' has no declaration", stmt);
        return;
    }

    // Register this impl with the trait
    traitSymbol->addImplDecl(stmt);

    // Empty impl blocks indicate intrinsic implementation
    // Example: impl:Numeric:for:int32 = {}
    // The compiler provides the operations as intrinsics, so no methods need to be defined
    if (stmt->methods.empty()) {
        // Intrinsic implementation - all trait methods assumed to be provided by compiler
        return;
    }

    // Check that all trait methods are implemented
    std::set<std::string> implementedMethods;
    for (const auto& methodNode : stmt->methods) {
        if (auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(methodNode)) {
            implementedMethods.insert(funcDecl->funcName);

            // Register the mangled function name: TypeName_methodName
            // This allows UFCS-style calls like object.method() -> TypeName_method(object)
            std::string mangledName = stmt->typeName + "_" + funcDecl->funcName;

            // Create a function type for the mangled function
            std::vector<Type*> paramTypes;
            for (const auto& param : funcDecl->parameters) {
                if (param->type == ASTNode::NodeType::PARAMETER) {
                    ParameterNode* pnode = static_cast<ParameterNode*>(param.get());
                    if (pnode->typeNode) {
                        std::string paramTypeName = pnode->typeNode->toString();
                        // Try struct type first (covers user-defined struct params like
                        // 'Counter:self'), then primitive, then i64 fallback
                        Type* ptype = typeSystem->getStructType(paramTypeName);
                        if (!ptype) ptype = typeSystem->getPrimitiveType(paramTypeName);
                        if (!ptype) ptype = typeSystem->getPrimitiveType("int64");  // fallback
                        paramTypes.push_back(ptype);
                    }
                }
            }

            std::string retTypeName = funcDecl->returnType ? funcDecl->returnType->toString() : "void";
            // Same struct-first lookup for the return type
            Type* retType = typeSystem->getStructType(retTypeName);
            if (!retType) retType = typeSystem->getPrimitiveType(retTypeName);
            if (!retType) retType = typeSystem->getPrimitiveType("void");

            // Impl methods are regular Aria functions — wrap return in Result<T>
            // (same as checkFuncDecl does for non-extern functions)
            Type* voidT = typeSystem->getPrimitiveType("void");
            if (funcDecl->body && !retType->equals(voidT) && retType->getKind() != TypeKind::RESULT) {
                retType = typeSystem->getResultType(retType);
            }

            Type* funcType = typeSystem->getFunctionType(paramTypes, retType, false);

            Symbol* funcSymbol = symbolTable->defineSymbol(mangledName, SymbolKind::FUNCTION, funcType);
            if (funcSymbol) {
                funcSymbol->setFuncDecl(funcDecl.get());
            }

            // Type check the method implementation
            checkFuncDecl(funcDecl.get());
        }
    }

    // Verify all required methods are present and signatures match
    for (const auto& requiredMethod : traitDecl->methods) {
        if (implementedMethods.find(requiredMethod.name) == implementedMethods.end()) {
            addError("Missing implementation of method '" + requiredMethod.name +
                    "' from trait '" + stmt->traitName + "' for type '" + stmt->typeName + "'", stmt);
            continue;
        }

        // Verify signature: parameter count and return type
        for (const auto& methodNode : stmt->methods) {
            auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(methodNode);
            if (!funcDecl || funcDecl->funcName != requiredMethod.name) continue;

            // Compare parameter count (trait 'self' is implicit in some contexts)
            size_t traitParamCount = requiredMethod.parameters.size();
            size_t implParamCount = funcDecl->parameters.size();
            if (implParamCount != traitParamCount) {
                addError("Method '" + requiredMethod.name + "' in impl for '" +
                        stmt->typeName + "' has " + std::to_string(implParamCount) +
                        " parameters, but trait '" + stmt->traitName + "' requires " +
                        std::to_string(traitParamCount), stmt);
            }

            // Compare return type
            std::string implRetType = funcDecl->returnType ? funcDecl->returnType->toString() : "void";
            if (!requiredMethod.returnType.empty() && implRetType != requiredMethod.returnType) {
                addError("Method '" + requiredMethod.name + "' in impl for '" +
                        stmt->typeName + "' returns '" + implRetType +
                        "', but trait '" + stmt->traitName + "' requires '" +
                        requiredMethod.returnType + "'", stmt);
            }
            break;
        }
    }
}

// ============================================================================
// Assignment Type Checking
// ============================================================================

void TypeChecker::checkAssignment(BinaryExpr* expr) {
    // Special case: Explicit discard with _
    if (expr->left->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr->left.get());
        if (ident->name == "_") {
            // Discard - just type check the right side, don't create symbol
            inferType(expr->right.get());
            return;
        }
    }
    
    // ========================================================================
    // Phase 1.5: Immutable .is_error Enforcement
    // ========================================================================
    // Block assignment to Result<T>.is_error unless variable has 'wild' modifier
    // Prevents error state corruption - errors are facts that must propagate
    if (expr->left->type == ASTNode::NodeType::MEMBER_ACCESS) {
        MemberAccessExpr* memberExpr = static_cast<MemberAccessExpr*>(expr->left.get());
        
        // Check if accessing .is_error field
        if (memberExpr->member == "is_error") {
            // Check if object is a Result type
            Type* objectType = inferType(memberExpr->object.get());
            if (objectType && objectType->getKind() == TypeKind::RESULT) {
                // Extract variable name
                std::string varName = getVariableName(memberExpr->object.get());
                
                // Check if Result variable has 'wild' modifier
                if (!varName.empty() && wildResults.find(varName) == wildResults.end()) {
                    addError(
                        "Cannot modify .is_error on non-wild Result type (errors must propagate).\n"
                        "  Variable: " + varName + "\n"
                        "  Help: Declare as 'wild Result<T>' if you need to modify error state.\n"
                        "  Note: Modifying error state is rarely needed and should be audited.",
                        expr->line, expr->column
                    );
                    return;
                }
            }
        }
    }
    
    // Left side must be an lvalue (identifier, index, member access, or dereference)
    bool isValidLvalue = false;
    
    if (expr->left->type == ASTNode::NodeType::IDENTIFIER ||
        expr->left->type == ASTNode::NodeType::INDEX ||
        expr->left->type == ASTNode::NodeType::MEMBER_ACCESS ||
        expr->left->type == ASTNode::NodeType::POINTER_MEMBER) {
        isValidLvalue = true;
    } else if (expr->left->type == ASTNode::NodeType::UNARY_OP) {
        // Allow dereference operators (<- and *) on left side
        // Example: <-ptr = 42; assigns through the pointer
        UnaryExpr* unary = static_cast<UnaryExpr*>(expr->left.get());
        if (unary->op.type == TokenType::TOKEN_LEFT_ARROW || 
            unary->op.type == TokenType::TOKEN_STAR) {
            isValidLvalue = true;
        }
    }
    
    if (!isValidLvalue) {
        addError("Left side of assignment must be a variable, array element, member, or pointer dereference", expr->left.get());
        return;
    }
    
    // Get left side type
    Type* leftType = inferType(expr->left.get());
    if (leftType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // Check if assigning to const variable
    if (expr->left->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr->left.get());
        Symbol* symbol = symbolTable->lookupSymbol(ident->name);
        
        if (symbol && symbol->kind == SymbolKind::CONSTANT) {
            addError("Cannot assign to const variable '" + ident->name + "'", expr);
            return;
        }
        
        // Check if assigning to fixed variable
        if (symbol && symbol->isFixed) {
            addError("Cannot reassign fixed variable '" + ident->name + "' - fixed variables are immutable", expr);
            return;
        }
        
        // Check if assigning through immutable borrow ($$i)
        if (symbol && symbol->isBorrowImm) {
            addError("Cannot assign through immutable borrow '" + ident->name + 
                     "' — use $$m for a mutable borrow", expr);
            return;
        }
    }
    
    // Check if modifying field of fixed variable
    if (expr->left->type == ASTNode::NodeType::MEMBER_ACCESS) {
        MemberAccessExpr* member = static_cast<MemberAccessExpr*>(expr->left.get());
        
        // Find the base identifier
        ASTNode* base = member->object.get();
        while (base->type == ASTNode::NodeType::MEMBER_ACCESS) {
            MemberAccessExpr* baseMember = static_cast<MemberAccessExpr*>(base);
            base = baseMember->object.get();
        }
        
        if (base->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* ident = static_cast<IdentifierExpr*>(base);
            Symbol* symbol = symbolTable->lookupSymbol(ident->name);
            
            if (symbol && symbol->isFixed) {
                addError("Cannot modify field of fixed variable '" + ident->name + "' - fixed variables are immutable", expr);
                return;
            }
        }
    }
    
    // Get right side type
    Type* rightType = inferType(expr->right.get());
    if (rightType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // Standard Integer Literal Assignment (Phase 4.5.4)
    // Allow safe narrowing for integer literals in assignments (e.g., x = 5)
    // This makes direct assignments consistent with variable declarations
    bool standardIntLiteralAssignment = false;
    if (isStandardIntType(leftType) && expr->right->type == ASTNode::NodeType::LITERAL) {
        LiteralExpr* literal = static_cast<LiteralExpr*>(expr->right.get());
        if (std::holds_alternative<int64_t>(literal->value)) {
            int64_t value = std::get<int64_t>(literal->value);
            if (literalFitsInType(value, leftType)) {
                // Literal fits, narrow the type to allow assignment
                rightType = leftType;
                standardIntLiteralAssignment = true;
            } else {
                // Literal doesn't fit, report error
                canLiteralFitInIntType(value, leftType, expr);
                return;
            }
        }
    }
    
    // TBB Type Validation (Phase 3.2.4)
    // Special handling for integer literals assigned to TBB types
    bool tbbLiteralAssignment = false;
    if (isTBBType(leftType) && expr->right->type == ASTNode::NodeType::LITERAL) {
        LiteralExpr* literal = static_cast<LiteralExpr*>(expr->right.get());
        if (std::holds_alternative<int64_t>(literal->value)) {
            int64_t value = std::get<int64_t>(literal->value);
            checkTBBLiteralValue(value, leftType, expr);
            tbbLiteralAssignment = true;
            
            // If validation failed, early exit
            // If succeeded, allow the assignment (literal is in range)
        }
    }
    
    // Balanced Type Validation (Phase 3.2.5)
    // Special handling for integer literals assigned to balanced types
    bool balancedLiteralAssignment = false;
    if (isBalancedType(leftType) && expr->right->type == ASTNode::NodeType::LITERAL) {
        LiteralExpr* literal = static_cast<LiteralExpr*>(expr->right.get());
        if (std::holds_alternative<int64_t>(literal->value)) {
            int64_t value = std::get<int64_t>(literal->value);
            checkBalancedLiteralValue(value, leftType, expr);
            balancedLiteralAssignment = true;
            
            // If validation failed, early exit
            // If succeeded, allow the assignment (literal is in range)
        }
    }
    
    // Check type compatibility (skip if we handled standard int, TBB, or balanced literal assignment)
    if (!standardIntLiteralAssignment && !tbbLiteralAssignment && !balancedLiteralAssignment) {
        // Special case: Allow T to be assigned to T? (wrapping)
        bool optionalWrapping = false;
        if (leftType->getKind() == TypeKind::OPTIONAL) {
            const OptionalType* leftOptional = static_cast<const OptionalType*>(leftType);
            if (rightType->isAssignableTo(leftOptional->getWrappedType())) {
                optionalWrapping = true;
            }
        }
        
        if (!rightType->isAssignableTo(leftType) && !canCoerce(rightType, leftType) && !optionalWrapping) {
            addError("Cannot assign value of type '" + rightType->toString() + 
                    "' to variable of type '" + leftType->toString() + "'", expr);
        }
    }
}

// ============================================================================
// Return Statement Type Checking
// ============================================================================

void TypeChecker::checkReturnStmt(ReturnStmt* stmt) {
    // Check if we're inside a function
    if (!currentFunctionReturnType) {
        addError("return statement outside of function", stmt);
        return;
    }
    
    // Get the primitive type "void" for comparison
    Type* voidType = typeSystem->getPrimitiveType("void");
    bool isVoidFunction = currentFunctionReturnType->equals(voidType);
    
    // Case 1: void function with return value
    if (isVoidFunction && stmt->value) {
        addError("NIL function cannot return a value. Use 'return;' or omit the return statement", stmt);
        return;
    }
    
    // Case 2: non-void function without return value
    if (!isVoidFunction && !stmt->value) {
        addError("Function must return a value of type '" + 
                currentFunctionReturnType->toString() + "'", stmt);
        return;
    }
    
    // Case 3: non-void function with return value - check type
    if (!isVoidFunction && stmt->value) {
        Type* returnType = inferType(stmt->value.get());
        
        if (returnType->getKind() == TypeKind::ERROR) {
            return;
        }
        
        // Context-aware literal typing for return statements
        // If returning an int64 literal to a smaller int type, check if it fits
        if (stmt->value->type == ASTNode::NodeType::LITERAL &&
            isStandardIntType(currentFunctionValueType) &&
            returnType->toString() == "int64") {
            
            LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->value.get());
            if (std::holds_alternative<int64_t>(literal->value)) {
                int64_t value = std::get<int64_t>(literal->value);
                if (literalFitsInType(value, currentFunctionValueType)) {
                    // Treat literal as having the function's value type
                    return;  // Success, literal fits
                }
            }
        }
        
        // Return statements should check against the VALUE type, not the Result<T> wrapper
        // Functions internally return Result<T>, but the return statement provides the value.
        // Auto-unwrap Result<T> when the caller returns a direct call to another Aria function.
        Type* effectiveReturnType = returnType;
        if (returnType->getKind() == TypeKind::RESULT) {
            ResultType* resultWrapper = static_cast<ResultType*>(returnType);
            effectiveReturnType = resultWrapper->getValueType();
        }
        if (!effectiveReturnType->isAssignableTo(currentFunctionValueType) && 
            !canCoerce(effectiveReturnType, currentFunctionValueType)) {
            addError("Return type '" + returnType->toString() + 
                    "' does not match function return type '" + 
                    currentFunctionValueType->toString() + "'", stmt);
        }
    }
}

// ============================================================================
// Pass/Fail Statement Type Checking (Result Types)
// ============================================================================

void TypeChecker::checkPassStmt(PassStmt* stmt) {
    // Check if we're inside a function
    if (!currentFunctionValueType) {
        addError("pass statement outside of function", stmt);
        return;
    }

    // pass is not allowed in main or failsafe — use exit instead
    if (currentFunctionName == "main" || currentFunctionName == "failsafe") {
        addError("'pass' cannot be used in '" + currentFunctionName + "'. "
                 "Use 'exit <code>' to terminate program endpoints.", stmt);
        return;
    }
    
    // Get the primitive type "NIL" for comparison
    Type* nilType = typeSystem->getPrimitiveType("NIL");
    bool isNilFunction = currentFunctionValueType->equals(nilType);
    
    // pass; (no value) — void return, only valid in NIL-returning functions
    if (!stmt->value) {
        if (!isNilFunction) {
            addError("'pass;' (no value) can only be used in functions returning NIL. "
                     "Function '" + currentFunctionName + "' returns " + 
                     currentFunctionValueType->toString() + ".", stmt);
        }
        return;
    }
    
    // Infer type of the pass value
    Type* valueType = inferType(stmt->value.get());
    
    if (valueType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // pass(value) builds Result{val: value, err: NULL}
    // Validate that value type matches the function's declared value type
    // func:foo = int32(...) { pass(42i32); } - int32 must match int32
    // func:bar = NIL(...) { pass(NIL); } - NIL must match NIL
    
    // Special case: Allow integer literals for exotic types (same as variable init)
    bool exoticLiteralPass = false;
    if ((isBalancedType(currentFunctionValueType) || isNumericExoticType(currentFunctionValueType) || isTBBType(currentFunctionValueType)) &&
        (valueType->toString() == "int32" || valueType->toString() == "int64")) {
        // Allow int32/int64 to be passed to exotic types (will be converted in IR gen)
        exoticLiteralPass = true;
    }
    
    // Special case: Allow integer literals for optional integer types
    bool optionalIntLiteralPass = false;
    if (currentFunctionValueType->getKind() == TypeKind::OPTIONAL) {
        const OptionalType* optType = static_cast<const OptionalType*>(currentFunctionValueType);
        Type* wrappedType = optType->getWrappedType();
        if (isStandardIntType(wrappedType) && 
            (valueType->toString() == "int32" || valueType->toString() == "int64")) {
            // Check if the literal value fits in the target type
            if (stmt->value->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->value.get());
                if (std::holds_alternative<int64_t>(literal->value)) {
                    int64_t value = std::get<int64_t>(literal->value);
                    if (literalFitsInType(value, wrappedType)) {
                        optionalIntLiteralPass = true;
                    }
                }
            }
        }
    }
    
    // Auto-unwrap Result<T> when the caller passes a direct call to another Aria function.
    Type* effectiveValueType = valueType;
    if (valueType->getKind() == TypeKind::RESULT) {
        ResultType* resultWrapper = static_cast<ResultType*>(valueType);
        effectiveValueType = resultWrapper->getValueType();
    }
    if (!exoticLiteralPass && !optionalIntLiteralPass && 
        !effectiveValueType->isAssignableTo(currentFunctionValueType) && 
        !canCoerce(effectiveValueType, currentFunctionValueType)) {
        addError("Pass value type '" + valueType->toString() + 
                "' does not match function value type '" + 
                currentFunctionValueType->toString() + "'", stmt);
    }
}

void TypeChecker::checkFailStmt(FailStmt* stmt) {
    // Check if we're inside a function
    if (!currentFunctionReturnType) {
        addError("fail statement outside of function", stmt);
        return;
    }
    
    // fail is not allowed in main or failsafe — use exit instead
    if (currentFunctionName == "main" || currentFunctionName == "failsafe") {
        addError("'fail' cannot be used in '" + currentFunctionName + "'. "
                 "Use 'exit <code>' to terminate program endpoints.", stmt);
        return;
    }
    
    // Infer type of the error code
    Type* errorType = inferType(stmt->errorCode.get());
    
    if (errorType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // fail(err) builds Result{val: NULL, err: err}
    // Error codes must be integer or TBB types (TBB types carry sentinel semantics
    // that align with Aria's error propagation model).
    if (errorType->getKind() != TypeKind::PRIMITIVE) {
        addError("Fail error code must be an integer or TBB type", stmt);
        return;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(errorType);
    if (!isStandardIntType(primType) && !primType->isTBBType()) {
        addError("Fail error code must be an integer or TBB type, got '" + errorType->toString() + "'", stmt);
    }
}

void TypeChecker::checkProveStmt(ProveStmt* stmt) {
    Type* condType = inferType(stmt->condition.get());
    
    if (condType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // Condition must be bool
    PrimitiveType* condPrim = dynamic_cast<PrimitiveType*>(condType);
    if (!condPrim || condPrim->getName() != "bool") {
        addError("prove condition must be 'bool' type, got '" + condType->toString() + "'", stmt);
    }
}

void TypeChecker::checkAssertStaticStmt(AssertStaticStmt* stmt) {
    Type* condType = inferType(stmt->condition.get());
    
    if (condType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // Condition must be bool
    PrimitiveType* condPrim = dynamic_cast<PrimitiveType*>(condType);
    if (!condPrim || condPrim->getName() != "bool") {
        addError("assert_static condition must be 'bool' type, got '" + condType->toString() + "'", stmt);
    }
}


// ============================================================================
// If Statement Type Checking
// ============================================================================

void TypeChecker::checkIfStmt(IfStmt* stmt) {
    // Check condition type
    Type* condType = inferType(stmt->condition.get());
    
    if (condType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // Condition must be bool (strict, no truthiness)
    PrimitiveType* condPrim = dynamic_cast<PrimitiveType*>(condType);
    if (!condPrim || condPrim->getName() != "bool") {
        addError("if condition must be 'bool' type, got '" + condType->toString() + 
                "'. Use explicit comparison (e.g., x != 0) instead of truthiness.", stmt->condition.get());
    }
    
    // Phase 2: Control flow analysis for Result checking
    // Save current state before branching
    ResultCheckState beforeState = currentResultState;
    
    // Analyze condition to extract Result check knowledge
    ResultCheckState thenState = beforeState;
    ResultCheckState elseState = beforeState;
    analyzeConditionForResultChecks(stmt->condition.get(), thenState, elseState);
    
    // Check then branch with then knowledge
    currentResultState = thenState;
    checkStatement(stmt->thenBranch.get());
    ResultCheckState afterThen = currentResultState;
    
    // Check else branch with else knowledge (if present)
    ResultCheckState afterElse;
    if (stmt->elseBranch) {
        currentResultState = elseState;
        checkStatement(stmt->elseBranch.get());
        afterElse = currentResultState;
    } else {
        // No else branch means execution can skip the if entirely
        afterElse = elseState;
    }
    
    // Early return analysis: if one branch always returns, use other branch's state
    bool thenReturns = branchAlwaysReturns(stmt->thenBranch.get());
    bool elseReturns = stmt->elseBranch && branchAlwaysReturns(stmt->elseBranch.get());
    
    if (thenReturns && !elseReturns) {
        // if (err) { return; } → after knows !err
        currentResultState = afterElse;
    } else if (elseReturns && !thenReturns) {
        // if (!err) { return; } → after knows err
        currentResultState = afterThen;
    } else if (thenReturns && elseReturns) {
        // Both branches diverge, execution doesn't continue
        // Keep merged state anyway for safety
        currentResultState = ResultCheckState::merge(afterThen, afterElse);
    } else {
        // Normal case: merge both branches conservatively
        currentResultState = ResultCheckState::merge(afterThen, afterElse);
    }
}

// ============================================================================
// While Statement Type Checking
// ============================================================================

void TypeChecker::checkWhileStmt(WhileStmt* stmt) {
    // Check condition type
    Type* condType = inferType(stmt->condition.get());
    
    if (condType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // Condition must be bool (strict, no truthiness)
    PrimitiveType* condPrim = dynamic_cast<PrimitiveType*>(condType);
    if (!condPrim || condPrim->getName() != "bool") {
        addError("while condition must be 'bool' type, got '" + condType->toString() + 
                "'. Use explicit comparison (e.g., x != 0) instead of truthiness.", stmt->condition.get());
    }
    
    // Phase 3: Control flow analysis for loops
    // Save current state before loop
    ResultCheckState beforeLoop = currentResultState;
    
    // Analyze condition to extract Result check knowledge
    ResultCheckState inLoop = beforeLoop;
    ResultCheckState afterLoop = beforeLoop;
    analyzeConditionForResultChecks(stmt->condition.get(), inLoop, afterLoop);
    
    // Check body with loop condition knowledge
    // (e.g., if while(!r.is_error), body knows KNOWN_SUCCESS)
    currentResultState = inLoop;
    checkStatement(stmt->body.get());
    
    // After loop: condition is FALSE (that's why we exited)
    // So use the 'else' state from condition analysis
    // But also be conservative: loop might not have executed at all (0 iterations)
    // Merge: what we knew before loop OR what condition being false proves
    currentResultState = ResultCheckState::merge(beforeLoop, afterLoop);
}

// ============================================================================
// For Statement Type Checking
// ============================================================================

void TypeChecker::checkForStmt(ForStmt* stmt) {
    if (stmt->isRangeBased) {
        // Range-based for loop: for (var in range) { body }
        // Check the range expression
        inferType(stmt->rangeExpr.get());
        // Range expressions currently always return a range struct, no need to validate further
        
        // Enter new scope for the loop body
        symbolTable->enterScope(ScopeKind::BLOCK, "forrange");
        
        // Add iterator variable to symbol table
        // If iteratorType is specified, use it; otherwise default to int64
        std::string iterType = stmt->iteratorType.empty() ? "int64" : stmt->iteratorType;
        Type* varType = typeSystem->getPrimitiveType(iterType);
        if (!varType || varType->getKind() == TypeKind::ERROR) {
            // Try as struct type
            varType = typeSystem->getStructType(iterType);
            if (!varType || varType->getKind() == TypeKind::ERROR) {
                addError("Unknown type for iterator: '" + iterType + "'", stmt);
                symbolTable->exitScope();
                return;
            }
        }
        
        // Define iterator as a variable in the loop scope
        symbolTable->defineSymbol(stmt->iteratorName, SymbolKind::VARIABLE, varType, 
                                  stmt->line, stmt->column);
        
        // Phase 3: Range-based loops are conservative
        // Save state before loop and reset after (can't track through iterations)
        ResultCheckState beforeLoop = currentResultState;
        
        // Check body with iterator in scope
        checkStatement(stmt->body.get());
        
        // Conservative: reset to state before loop (don't know how many iterations)
        currentResultState = beforeLoop;
        
        // Exit scope
        symbolTable->exitScope();
        return;
    }
    
    // C-style for loop
    // Phase 3: Save state before loop
    ResultCheckState beforeLoop = currentResultState;
    
    // Check initializer if present
    if (stmt->initializer) {
        checkStatement(stmt->initializer.get());
    }
    
    // Analyze condition if present
    ResultCheckState inLoop = currentResultState;
    ResultCheckState afterLoop = currentResultState;
    
    if (stmt->condition) {
        Type* condType = inferType(stmt->condition.get());
        
        if (condType->getKind() != TypeKind::ERROR) {
            PrimitiveType* condPrim = dynamic_cast<PrimitiveType*>(condType);
            if (!condPrim || condPrim->getName() != "bool") {
                addError("for condition must be 'bool' type, got '" + condType->toString() + 
                        "'. Use explicit comparison (e.g., i < 10) instead of truthiness.", stmt->condition.get());
            }
        }
        
        // Analyze condition for Result checks (like while loops)
        analyzeConditionForResultChecks(stmt->condition.get(), inLoop, afterLoop);
    }
    
    // Check body with loop condition knowledge
    currentResultState = inLoop;
    
    // Check update if present (just infer type, any expression is valid)
    if (stmt->update) {
        inferType(stmt->update.get());
    }
    
    // Check body
    checkStatement(stmt->body.get());
    
    // After loop: merge conservative (might not have executed) with exit condition
    currentResultState = ResultCheckState::merge(beforeLoop, afterLoop);
}

// ============================================================================
// Block Statement Type Checking
// ============================================================================

void TypeChecker::checkBlockStmt(BlockStmt* stmt) {
    // Enter new scope
    symbolTable->enterScope(ScopeKind::BLOCK, "block");
    
    // Check all statements in block
    for (const auto& statement : stmt->statements) {
        checkStatement(statement.get());
    }
    
    // Exit scope
    symbolTable->exitScope();
}

// ============================================================================
// Expression Statement Type Checking
// ============================================================================

void TypeChecker::checkExpressionStmt(ExpressionStmt* stmt) {
    // Special case: check if it's an assignment (including explicit discard)
    if (stmt->expression->type == ASTNode::NodeType::BINARY_OP) {
        BinaryExpr* binExpr = static_cast<BinaryExpr*>(stmt->expression.get());
        if (binExpr->op.type == frontend::TokenType::TOKEN_EQUAL) {
            // Check for explicit discard syntax: _ = expr
            if (binExpr->left->type == ASTNode::NodeType::IDENTIFIER) {
                IdentifierExpr* ident = static_cast<IdentifierExpr*>(binExpr->left.get());
                if (ident->name == "_") {
                    // Explicit discard - infer type but skip nodiscard check
                    inferType(binExpr->right.get());
                    return;
                }
            }
            
            // Regular assignment
            checkAssignment(binExpr);
            return;
        }
    }
    
    // Otherwise just infer type (to check for errors)
    Type* exprType = inferType(stmt->expression.get());
    
    // Phase 2.1: Must-Use Result Analysis
    // Check if expression has nodiscard type and value is unused
    if (exprType && exprType->isNodiscard()) {
        addError("Unused result value. This function returns a result that must be checked.",
              stmt->expression.get());
    }
}

// ============================================================================
// TBB Type Validation - Phase 3.2.4
// ============================================================================

bool TypeChecker::isTBBType(Type* type) {
    if (!type || type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& name = primType->getName();
    
    return name == "tbb8" || name == "tbb16" || name == "tbb32" || name == "tbb64";
}

int64_t TypeChecker::getTBBErrorSentinel(Type* type) {
    if (!isTBBType(type)) {
        return 0;  // Not a TBB type
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& name = primType->getName();
    
    if (name == "tbb8") {
        return -128;  // 0x80
    } else if (name == "tbb16") {
        return -32768;  // 0x8000
    } else if (name == "tbb32") {
        return -2147483648LL;  // 0x80000000
    } else if (name == "tbb64") {
        return INT64_MIN;  // 0x8000000000000000
    }
    
    return 0;
}

std::pair<int64_t, int64_t> TypeChecker::getTBBValidRange(Type* type) {
    if (!isTBBType(type)) {
        return {0, 0};  // Not a TBB type
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& name = primType->getName();
    
    if (name == "tbb8") {
        return {-127, 127};
    } else if (name == "tbb16") {
        return {-32767, 32767};
    } else if (name == "tbb32") {
        return {-2147483647LL, 2147483647LL};
    } else if (name == "tbb64") {
        return {-9223372036854775807LL, 9223372036854775807LL};
    }
    
    return {0, 0};
}

void TypeChecker::checkTBBLiteralValue(int64_t value, Type* type, ASTNode* node) {
    if (!isTBBType(type)) {
        return;  // Not a TBB type, no validation needed
    }
    
    int64_t errSentinel = getTBBErrorSentinel(type);
    
    // Check if value is the ERR sentinel
    if (value == errSentinel) {
        PrimitiveType* primType = static_cast<PrimitiveType*>(type);
        addWarning("Assigning ERR sentinel value (" + std::to_string(errSentinel) + 
                ") to " + primType->getName() + ". Use 'ERR' keyword for clarity.", node);
    }
    
    // Check if value is in valid range
    auto [minVal, maxVal] = getTBBValidRange(type);
    if (value < minVal || value > maxVal) {
        // Value is out of range (excluding ERR sentinel check above)
        if (value != errSentinel) {
            PrimitiveType* primType = static_cast<PrimitiveType*>(type);
            addError("Value " + std::to_string(value) + " is out of range for " + 
                    primType->getName() + " (valid range: [" + std::to_string(minVal) + 
                    ", " + std::to_string(maxVal) + "])", node);
        }
    }
}

bool TypeChecker::isERRProducingOperation(Type* resultType, Type* leftType, Type* rightType) {
    // If any operand is a TBB type, the result inherits TBB semantics
    // In actual runtime, ERR values propagate through operations
    // For type checking, we just verify the types are compatible
    
    // Note: This method is primarily for documentation and future expansion
    // The actual ERR propagation happens at runtime
    // Type checker just ensures TBB types are used correctly
    
    if (!isTBBType(resultType)) {
        return false;
    }
    
    // TBB + TBB = TBB (ERR propagates at runtime)
    // TBB + standard int = ERROR (mixing not allowed)
    if (isTBBType(leftType) != isTBBType(rightType)) {
        return true;  // Type mismatch will be caught by type checking
    }
    
    return false;
}

// ============================================================================
// Balanced Ternary/Nonary Type Validation - Phase 3.2.5
// ============================================================================

bool TypeChecker::isBalancedType(Type* type) {
    if (!type || type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& name = primType->getName();
    
    return name == "trit" || name == "tryte" || name == "nit" || name == "nyte";
}

bool TypeChecker::isTritType(Type* type) {
    if (!type || type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    return primType->getName() == "trit";
}

bool TypeChecker::isTryteType(Type* type) {
    if (!type || type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    return primType->getName() == "tryte";
}

bool TypeChecker::isNitType(Type* type) {
    if (!type || type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    return primType->getName() == "nit";
}

bool TypeChecker::isNyteType(Type* type) {
    if (!type || type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    return primType->getName() == "nyte";
}

std::vector<int> TypeChecker::getBalancedValidDigits(Type* type) {
    if (isTritType(type)) {
        // trit: balanced ternary digit {-1, 0, 1}
        return {-1, 0, 1};
    } else if (isNitType(type)) {
        // nit: balanced nonary digit {-4, -3, -2, -1, 0, 1, 2, 3, 4}
        return {-4, -3, -2, -1, 0, 1, 2, 3, 4};
    }
    
    // Composite types (tryte/nyte) don't have explicit digit sets
    return {};
}

bool TypeChecker::isNumericExoticType(Type* type) {
    if (!type || type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& name = primType->getName();
    
    // Fractional types (exact rational arithmetic)
    if (name == "frac8" || name == "frac16" || name == "frac32" || name == "frac64") {
        return true;
    }
    
    // Twisted floating point types
    if (name == "tfp32" || name == "tfp64") {
        return true;
    }
    
    // 9D toroidal types
    if (name == "vec9" || name == "dvec9" || name == "tmatrix" || name == "ttensor") {
        return true;
    }
    
    return false;
}

// P1-5: Helper to check if a type is numeric (for dimensional analysis)
bool TypeChecker::isNumericType(Type* type) {
    if (!type) {
        return false;
    }
    
    // Check for primitive numeric types
    if (type->getKind() == TypeKind::PRIMITIVE) {
        PrimitiveType* primType = static_cast<PrimitiveType*>(type);
        const std::string& name = primType->getName();
        
        // Standard integers (both canonical and alias forms)
        // Canonical: int8, int16, int32, int64, uint8, uint16, uint32, uint64
        // Alias: Same names supported
        if (name.find("int") == 0 || name.find("uint") == 0) {
            return true;
        }
        
        // Standard floats (both canonical and alias forms)
        // Canonical: flt32, flt64, flt128, flt256, flt512
        // Alias: float32, float64, float128, float256, float512
        if (name.find("flt") == 0 || name.find("float") == 0) {
            return true;
        }
        
        // Fixed-point types
        if (name.find("fix") == 0) {
            return true;
        }
        
        // TBB types
        if (name.find("tbb") == 0) {
            return true;
        }
        
        // Exotic numeric types
        if (isNumericExoticType(type)) {
            return true;
        }
        
        // Balanced ternary/nonary types
        if (name == "trit" || name == "tryte" || name == "nit" || name == "nyte") {
            return true;
        }
        
        // Vector types
        if (name == "vec9" || name == "dvec9") {
            return true;
        }
    }
    
    // Check for struct types that represent numeric types (like custom fixed-point)
    if (type->getKind() == TypeKind::STRUCT) {
        StructType* structType = static_cast<StructType*>(type);
        const std::string& name = structType->getName();
        
        // Fixed-point types (if defined as structs)
        if (name.find("fix") == 0) {
            return true;
        }
    }
    
    return false;
}

std::pair<int64_t, int64_t> TypeChecker::getBalancedCompositeRange(Type* type) {
    if (isTryteType(type) || isNyteType(type)) {
        // Both tryte and nyte have the same range
        // tryte: 10 trits = 3^10 = 59,049 values
        // nyte: 5 nits = 9^5 = 59,049 values
        // Range: [-29524, +29524]
        return {-29524, 29524};
    }
    
    // Atomic types (trit/nit) use getBalancedValidDigits instead
    return {0, 0};
}

void TypeChecker::checkBalancedLiteralValue(int64_t value, Type* type, ASTNode* node) {
    if (!isBalancedType(type)) {
        return;  // Not a balanced type, no validation needed
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& typeName = primType->getName();
    
    // Check atomic types (trit, nit)
    if (isTritType(type)) {
        // trit must be exactly -1, 0, 1, or -128 (TRIT_ERR sentinel)
        if (value != -1 && value != 0 && value != 1 && value != -128) {
            addError("trit value must be -1, 0, 1, or -128 (ERR), got " + std::to_string(value), node);
        }
    } else if (isNitType(type)) {
        // nit must be -4 to +4, or -128 (NIT_ERR sentinel)
        if ((value < -4 || value > 4) && value != -128) {
            addError("nit value must be in range [-4, 4] or -128 (ERR), got " + std::to_string(value), node);
        }
    } else if (isTryteType(type)) {
        // tryte: composite type, check range
        auto [minVal, maxVal] = getBalancedCompositeRange(type);
        if (value < minVal || value > maxVal) {
            addError("tryte value must be in range [" + std::to_string(minVal) + 
                    ", " + std::to_string(maxVal) + "], got " + std::to_string(value), node);
        }
    } else if (isNyteType(type)) {
        // nyte: composite type, check range
        auto [minVal, maxVal] = getBalancedCompositeRange(type);
        if (value < minVal || value > maxVal) {
            addError("nyte value must be in range [" + std::to_string(minVal) + 
                    ", " + std::to_string(maxVal) + "], got " + std::to_string(value), node);
        }
    }
}

// ============================================================================
// Standard Integer Type Validation
// ============================================================================

bool TypeChecker::isStandardIntType(Type* type) const {
    if (type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& name = primType->getName();
    
    return name == "int1" || name == "int2" || name == "int4" ||
           name == "int8" || name == "int16" || name == "int32" || name == "int64" ||
           name == "uint1" || name == "uint2" || name == "uint4" ||
           name == "uint8" || name == "uint16" || name == "uint32" || name == "uint64";
}

bool TypeChecker::literalFitsInType(int64_t value, Type* type) const {
    if (!isStandardIntType(type)) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& typeName = primType->getName();
    
    // Determine range based on type name
    if (typeName == "int1") {
        return value >= -1 && value <= 0;
    } else if (typeName == "int2") {
        return value >= -2 && value <= 1;
    } else if (typeName == "int4") {
        return value >= -8 && value <= 7;
    } else if (typeName == "int8") {
        return value >= -128 && value <= 127;
    } else if (typeName == "int16") {
        return value >= -32768 && value <= 32767;
    } else if (typeName == "int32") {
        return value >= -2147483648LL && value <= 2147483647LL;
    } else if (typeName == "int64") {
        return true;  // int64 can hold any int64_t value
    } else if (typeName == "uint1") {
        return value >= 0 && value <= 1;
    } else if (typeName == "uint2") {
        return value >= 0 && value <= 3;
    } else if (typeName == "uint4") {
        return value >= 0 && value <= 15;
    } else if (typeName == "uint8") {
        return value >= 0 && value <= 255;
    } else if (typeName == "uint16") {
        return value >= 0 && value <= 65535;
    } else if (typeName == "uint32") {
        return value >= 0 && value <= 4294967295LL;
    } else if (typeName == "uint64") {
        return true;  // Any int64_t bit pattern is a valid uint64 value.
                      // Values >= 2^63 are stored as negative int64_t (bit-reinterp from stoull).
    }
    
    return false;  // Unknown type
}

bool TypeChecker::canLiteralFitInIntType(int64_t value, Type* type, ASTNode* node) {
    if (!isStandardIntType(type)) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& typeName = primType->getName();
    
    int64_t minVal, maxVal;
    
    // Determine range based on type name
    if (typeName == "int1") {
        minVal = -1;
        maxVal = 0;
    } else if (typeName == "int2") {
        minVal = -2;
        maxVal = 1;
    } else if (typeName == "int4") {
        minVal = -8;
        maxVal = 7;
    } else if (typeName == "int8") {
        minVal = -128;
        maxVal = 127;
    } else if (typeName == "int16") {
        minVal = -32768;
        maxVal = 32767;
    } else if (typeName == "int32") {
        minVal = -2147483648LL;
        maxVal = 2147483647LL;
    } else if (typeName == "int64") {
        // int64 can hold any int64_t value
        return true;
    } else if (typeName == "int128" || typeName == "int256" || 
               typeName == "int512" || typeName == "int1024" ||
               typeName == "int2048" || typeName == "int4096") {
        // Big integer types: if it fits in int64, it fits in these
        return true;
    } else if (typeName == "uint1") {
        if (value < 0) {
            addError("Cannot assign negative value " + std::to_string(value) + " to unsigned type " + typeName, node);
            return false;
        }
        minVal = 0;
        maxVal = 1;
    } else if (typeName == "uint2") {
        if (value < 0) {
            addError("Cannot assign negative value " + std::to_string(value) + " to unsigned type " + typeName, node);
            return false;
        }
        minVal = 0;
        maxVal = 3;
    } else if (typeName == "uint4") {
        if (value < 0) {
            addError("Cannot assign negative value " + std::to_string(value) + " to unsigned type " + typeName, node);
            return false;
        }
        minVal = 0;
        maxVal = 15;
    } else if (typeName == "uint8") {
        if (value < 0) {
            addError("Cannot assign negative value " + std::to_string(value) + " to unsigned type " + typeName, node);
            return false;
        }
        minVal = 0;
        maxVal = 255;
    } else if (typeName == "uint16") {
        if (value < 0) {
            addError("Cannot assign negative value " + std::to_string(value) + " to unsigned type " + typeName, node);
            return false;
        }
        minVal = 0;
        maxVal = 65535;
    } else if (typeName == "uint32") {
        if (value < 0) {
            addError("Cannot assign negative value " + std::to_string(value) + " to unsigned type " + typeName, node);
            return false;
        }
        minVal = 0;
        maxVal = 4294967295LL;
    } else if (typeName == "uint64") {
        // Any int64_t bit pattern is a valid uint64 value.
        // Values >= 2^63 are stored as negative int64_t after stoull bit-reinterpretation
        // in the lexer (Bug #21 fix). Rejecting negative stored values here would wrongly
        // block legal uint64 literals like 9223372036854775808u64.
        return true;
    } else if (typeName == "uint128" || typeName == "uint256" || 
               typeName == "uint512" || typeName == "uint1024" ||
               typeName == "uint2048" || typeName == "uint4096") {
        if (value < 0) {
            addError("Cannot assign negative value " + std::to_string(value) + " to unsigned type " + typeName, node);
            return false;
        }
        // Big unsigned types: if it's non-negative and fits in int64, it fits
        return true;
    } else {
        return false;  // Unknown type
    }
    
    // Check if value is in range
    if (value < minVal || value > maxVal) {
        addError("Value " + std::to_string(value) + " is out of range for " + 
                typeName + " (valid range: [" + std::to_string(minVal) + 
                ", " + std::to_string(maxVal) + "])", node);
        return false;
    }
    
    return true;
}

// ============================================================================
// Module System Type Checking
// ============================================================================

void TypeChecker::checkUseStmt(UseStmt* stmt) {
    if (!stmt) {
        return;
    }
    
    // Basic validation of use statement structure
    if (stmt->path.empty()) {
        addError("Empty module path in use statement", stmt);
        return;
    }
    
    // Validate selective imports and wildcards
    if (stmt->isWildcard && !stmt->items.empty()) {
        addError("Cannot have both wildcard (*) and selective imports in use statement", stmt);
        return;
    }
    
    // If no module loader is available, skip module loading (used in tests)
    if (!moduleLoader) {
        return;
    }
    
    // Convert path vector to logical path string (e.g., ["std", "io"] -> "std.io")
    std::string logicalPath;
    if (stmt->isFilePath) {
        // File paths are already in the first element
        logicalPath = stmt->path[0];
    } else {
        // Join path components with dots
        for (size_t i = 0; i < stmt->path.size(); ++i) {
            if (i > 0) logicalPath += ".";
            logicalPath += stmt->path[i];
        }
    }
    
    // Load the module using the UseStmt overload so that isFilePath is preserved.
    // The string-based overload re-derives isFilePath from the string content,
    // which fails for "wave.aria" (no slash, no leading dot) — it would be treated
    // as a logical dotted path instead of a file path.
    LoadedModule* module = moduleLoader->loadModule(stmt, currentModulePath);
    if (!module) {
        addError("Failed to load module '" + logicalPath + "'", stmt);
        // Add detailed errors from module loader
        for (const auto& err : moduleLoader->getErrors()) {
            addError("  " + err, stmt);
        }
        return;
    }
    
    // Type-check the loaded module if not already done
    // This populates the module's symbol table and exports
    if (module->state == ModuleState::FULLY_LOADED && module->moduleInfo) {
        // Check if we've already type-checked this module
        // (Simple check: if exports is empty, we haven't type-checked yet)
        if (module->moduleInfo->getExports().empty() && module->ast) {
            // Temporarily redirect currentModulePath to the module being processed
            // so that relative 'use' statements INSIDE that module (e.g. wavemech.aria
            // doing 'use "wave.aria".*;') resolve relative to the module's own directory.
            std::string savedModulePath = currentModulePath;
            currentModulePath = module->canonicalPath;

            // IMPORT PASS: Process any 'use' statements at the top of the module first
            // so that symbols from dependencies are in scope during type-checking.
            for (const auto& decl : module->ast->declarations) {
                if (decl->type == ASTNode::NodeType::USE) {
                    checkUseStmt(static_cast<UseStmt*>(decl.get()));
                }
            }

            // STRUCT PRE-PASS: register non-generic struct types BEFORE function
            // signatures so that struct names resolve correctly in param/return types.
            // Also register Type:Name struct layouts early for the same reason.
            for (const auto& decl : module->ast->declarations) {
                if (decl->type == ASTNode::NodeType::STRUCT_DECL) {
                    StructDeclStmt* sd = static_cast<StructDeclStmt*>(decl.get());
                    if (sd->genericParams.empty() && !typeSystem->getStructType(sd->structName)) {
                        checkStructDecl(sd);
                    }
                } else if (decl->type == ASTNode::NodeType::TYPE_DECL) {
                    TypeDeclStmt* td = static_cast<TypeDeclStmt*>(decl.get());
                    if (!typeSystem->getStructType(td->typeName)) {
                        // Pre-register the merged struct layout so method parameter/return
                        // types that reference TypeName (e.g. Number:self) can be resolved.
                        std::vector<ASTNodePtr> preFields;
                        if (td->internalStruct) {
                            auto is = std::static_pointer_cast<StructDeclStmt>(td->internalStruct);
                            for (const auto& f : is->fields) preFields.push_back(f);
                        }
                        if (td->interfaceStruct) {
                            auto is = std::static_pointer_cast<StructDeclStmt>(td->interfaceStruct);
                            for (const auto& f : is->fields) preFields.push_back(f);
                        }
                        auto preStruct = std::make_shared<StructDeclStmt>(
                            td->typeName, preFields, td->line, td->column);
                        checkStructDecl(preStruct.get());
                    }
                }
            }

            // PRE-PASS: Register all global variable and constant declarations first.
            // This must happen before type-checking functions, because module-level
            // functions may reference global variables defined in the same module.
            // Also register ALL function signatures (inc. private helpers) so that
            // public functions calling private helpers can resolve their types.
            // Note: STRUCT_DECL is handled in the main pass below (not idempotent).
            // Note: Constants use VAR_DECL with isConst=true — no separate node type.
            for (const auto& decl : module->ast->declarations) {
                if (decl->type == ASTNode::NodeType::VAR_DECL) {
                    checkVarDecl(static_cast<VarDeclStmt*>(decl.get()));
                } else if (decl->type == ASTNode::NodeType::FUNC_DECL) {
                    // Register ALL functions (public and private) so calls to private
                    // helpers inside public functions resolve correctly.
                    FuncDeclStmt* fd = static_cast<FuncDeclStmt*>(decl.get());
                    if (!fd->genericParams.empty()) continue; // generics handled later
                    std::vector<Type*> pts;
                    for (const auto& p : fd->parameters) {
                        if (p->type == ASTNode::NodeType::PARAMETER) {
                            ParameterNode* pn = static_cast<ParameterNode*>(p.get());
                            Type* pt = resolveTypeNode(pn->typeNode.get());
                            pts.push_back((pt && pt->getKind() != TypeKind::ERROR)
                                          ? pt : typeSystem->getPrimitiveType("void"));
                        }
                    }
                    // Return type must be wrapped in Result<T> for body functions,
                    // same as the standalone program pre-pass (see preRegisterFunctions).
                    Type* valueType = resolveTypeNode(fd->returnType.get());
                    if (!valueType) valueType = typeSystem->getPrimitiveType("void");
                    Type* retType = fd->body ? new ResultType(valueType) : valueType;
                    Type* ft = new FunctionType(pts, retType, fd->isAsync, fd->isVariadic);
                    // Only define if not already in symbol table
                    if (!symbolTable->lookupSymbol(fd->funcName)) {
                        Symbol* sym = symbolTable->defineSymbol(fd->funcName,
                                          SymbolKind::FUNCTION, ft, fd->line, fd->column);
                        if (sym) sym->setFuncDecl(fd);
                    }
                } else if (decl->type == ASTNode::NodeType::EXTERN) {
                    // BUG-002 fix: Register extern block functions in symbol table
                    // so pub func wrappers in this module can reference them.
                    checkExternStmt(static_cast<ExternStmt*>(decl.get()));
                }
            }

            // MAIN PASS: Export PUBLIC top-level declarations
            for (const auto& decl : module->ast->declarations) {
                if (decl->type == ASTNode::NodeType::FUNC_DECL) {
                    FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(decl.get());
                    
                    // Only export public functions
                    if (!funcDecl->isPublic) {
                        continue;
                    }
                    
                    // Type-check the function declaration (handles both generic and non-generic)
                    // This registers generic functions for later monomorphization
                    checkFuncDecl(funcDecl);
                    
                    // Build the function type from the declaration
                    // First, resolve parameter types
                    std::vector<Type*> paramTypes;
                    
                    if (!funcDecl->genericParams.empty()) {
                        // Generic function - use placeholder types
                        for (const auto& param : funcDecl->parameters) {
                            paramTypes.push_back(typeSystem->getUnknownType());
                        }
                    } else {
                        // Non-generic function - resolve actual types
                        for (const auto& param : funcDecl->parameters) {
                            if (param->type == ASTNode::NodeType::PARAMETER) {
                                ParameterNode* paramNode = static_cast<ParameterNode*>(param.get());
                                Type* paramType = resolveTypeNode(paramNode->typeNode.get());
                                if (paramType && paramType->getKind() != TypeKind::ERROR) {
                                    paramTypes.push_back(paramType);
                                } else {
                                    // If we can't resolve param type, use void as fallback
                                    paramTypes.push_back(typeSystem->getPrimitiveType("void"));
                                }
                            }
                        }
                    }
                    
                    // Resolve return type
                    Type* returnType = nullptr;
                    if (!funcDecl->genericParams.empty()) {
                        // Generic function - use placeholder return type
                        returnType = typeSystem->getUnknownType();
                    } else {
                        returnType = resolveTypeNode(funcDecl->returnType.get());
                        if (!returnType || returnType->getKind() == TypeKind::ERROR) {
                            returnType = typeSystem->getPrimitiveType("void");
                        }
                        // Non-extern functions use pass() which wraps
                        // the return value in Result<T>. The export must mirror this
                        // so importers see Result<T>, not the raw declared type.
                        if (funcDecl->body) {
                            returnType = new ResultType(returnType);
                        }
                    }
                    
                    // Create the function type
                    Type* funcType = new FunctionType(paramTypes, returnType, funcDecl->isAsync, funcDecl->isVariadic);
                    
                    // Create a new symbol for export
                    Symbol* funcSym = new Symbol(funcDecl->funcName, SymbolKind::FUNCTION, funcType, nullptr, funcDecl->line, funcDecl->column);
                    funcSym->funcDecl = funcDecl;  // Store AST for generic resolution
                    
                    // Export the function as PUBLIC
                    module->moduleInfo->exportSymbol(funcDecl->funcName, funcSym, Visibility::PUBLIC);
                }
                else if (decl->type == ASTNode::NodeType::STRUCT_DECL) {
                    StructDeclStmt* structDecl = static_cast<StructDeclStmt*>(decl.get());
                    
                    // Type-check the struct declaration to register generic structs
                    // and create proper types in the type system
                    checkStructDecl(structDecl);
                    
                    // Get the actual struct type from the type system
                    Type* structType = nullptr;
                    if (!structDecl->genericParams.empty()) {
                        // Generic struct - it's registered but not instantiated yet
                        structType = typeSystem->getPrimitiveType("void");  // Placeholder for generic
                    } else {
                        // Non-generic struct - should be in type system now
                        structType = typeSystem->getStructType(structDecl->structName);
                        if (!structType) {
                            structType = typeSystem->getPrimitiveType("void");  // Fallback
                        }
                    }
                    
                    Symbol* structSym = new Symbol(structDecl->structName, SymbolKind::TYPE, structType, nullptr, structDecl->line, structDecl->column);
                    
                    // Export the struct based on its visibility.
                    // pub struct: → PUBLIC, plain struct: → PRIVATE
                    Visibility vis = structDecl->isPublic ? Visibility::PUBLIC : Visibility::PRIVATE;
                    module->moduleInfo->exportSymbol(structDecl->structName, structSym, vis);
                }
                else if (decl->type == ASTNode::NodeType::TYPE_DECL) {
                    TypeDeclStmt* typeDecl = static_cast<TypeDeclStmt*>(decl.get());

                    // The struct layout was pre-registered in the struct pre-pass above.
                    // Here we rename and type-check each method function, then export
                    // both the struct type and all mangled method names.
                    //
                    // We do NOT call checkTypeDecl() because it would re-register the
                    // struct and error with "Type already defined".

                    // Helper: rename func → TypeName_func, type-check, export
                    auto processAndExport = [&](FuncDeclStmt* func) {
                        std::string mangledName = typeDecl->typeName + "_" + func->funcName;
                        func->funcName = mangledName;
                        checkFuncDecl(func);
                        Symbol* sym = symbolTable->lookupSymbol(mangledName);
                        if (sym) {
                            module->moduleInfo->exportSymbol(mangledName, sym, Visibility::PUBLIC);
                        }
                    };

                    if (typeDecl->createFunc) {
                        auto cf = std::static_pointer_cast<FuncDeclStmt>(typeDecl->createFunc);
                        processAndExport(cf.get());
                    }
                    if (typeDecl->destroyFunc) {
                        auto df = std::static_pointer_cast<FuncDeclStmt>(typeDecl->destroyFunc);
                        processAndExport(df.get());
                    }
                    for (const auto& method : typeDecl->methods) {
                        auto mf = std::static_pointer_cast<FuncDeclStmt>(method);
                        processAndExport(mf.get());
                    }

                    // Export the struct type itself (e.g. "Number")
                    Type* typeStructType = typeSystem->getStructType(typeDecl->typeName);
                    if (typeStructType) {
                        Symbol* typeSym = new Symbol(typeDecl->typeName, SymbolKind::TYPE,
                                                     typeStructType, nullptr,
                                                     typeDecl->line, typeDecl->column);
                        module->moduleInfo->exportSymbol(typeDecl->typeName, typeSym, Visibility::PUBLIC);
                    }
                }
                else if (decl->type == ASTNode::NodeType::EXTERN) {
                    // BUG-002 fix: Export extern block functions so importers can
                    // reference them (both directly and through pub func wrappers).
                    // Extern functions use raw return types (no Result<T> wrapping).
                    ExternStmt* externStmt = static_cast<ExternStmt*>(decl.get());
                    for (const auto& extDecl : externStmt->declarations) {
                        if (extDecl->type == ASTNode::NodeType::FUNC_DECL) {
                            FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(extDecl.get());
                            // Look up the symbol registered by checkExternStmt in the pre-pass
                            Symbol* sym = symbolTable->lookupSymbol(funcDecl->funcName);
                            if (sym) {
                                module->moduleInfo->exportSymbol(funcDecl->funcName, sym, Visibility::PUBLIC);
                            }
                        }
                    }
                }
            }
            
            // Mark module as type-checked to avoid re-processing
            module->state = ModuleState::CHECKED;

            // Restore the caller's module path
            currentModulePath = savedModulePath;
        }
    }
    
    // Import symbols based on import type
    if (stmt->isWildcard) {
        // Wildcard import: import all public symbols directly into current scope
        importWildcardSymbols(module, stmt);
    } else if (!stmt->items.empty()) {
        // Selective import: import only specified symbols
        importSelectiveSymbols(module, stmt);
    } else {
        // Namespace import: import module as a namespace
        importModuleNamespace(module, stmt, logicalPath);
    }
}

void TypeChecker::checkModStmt(ModStmt* stmt) {
    if (!stmt) {
        return;
    }
    
    // Validate module name
    if (stmt->name.empty()) {
        addError("Module name cannot be empty", stmt);
        return;
    }
    
    // Check for invalid characters in module name
    // Module names should be valid identifiers
    for (char c : stmt->name) {
        if (!std::isalnum(c) && c != '_') {
            addError("Invalid character '" + std::string(1, c) + "' in module name '" + 
                    stmt->name + "'. Module names must be valid identifiers.", stmt);
            return;
        }
    }
    
    // If it's an inline module, type check all declarations in its body
    if (stmt->isInline) {
        // Enter module scope (for nested symbol resolution)
        // For now, just check each declaration
        for (const auto& decl : stmt->body) {
            if (decl) {
                checkStatement(decl.get());
            }
        }
    }
    
    // External module declarations (mod name;) don't need further checking
    // The module resolver will validate they exist
}

// ============================================================================
// Transitive Monomorphization — analyze specialized function bodies
// ============================================================================

void TypeChecker::analyzeSpecializedBody(FuncDeclStmt* specDecl, const TypeSubstitution& outerSub) {
    if (!specDecl || !specDecl->body || !genericResolver || !monomorphizer) return;

    // Recursive AST walker to find CallExpr nodes referencing generic functions
    std::function<void(ASTNode*)> walkNode = [&](ASTNode* node) {
        if (!node) return;

        if (node->type == ASTNode::NodeType::CALL) {
            CallExpr* call = static_cast<CallExpr*>(node);
            if (call->callee && call->callee->type == ASTNode::NodeType::IDENTIFIER) {
                IdentifierExpr* id = static_cast<IdentifierExpr*>(call->callee.get());

                // Check if this callee is a generic function
                Symbol* sym = symbolTable->lookupSymbol(id->name);
                FuncDeclStmt* calledFunc = nullptr;
                if (sym && sym->kind == SymbolKind::FUNCTION) {
                    calledFunc = sym->getFuncDecl();
                }
                if (calledFunc && !calledFunc->genericParams.empty() &&
                    call->specializedMangledName.empty()) {
                    // Build substitution for the nested generic call using
                    // the outer specialization's type mapping.
                    TypeSubstitution sub;
                    for (const auto& gp : calledFunc->genericParams) {
                        auto it = outerSub.find(gp.name);
                        if (it != outerSub.end()) {
                            sub[gp.name] = it->second;
                        }
                    }

                    if (!sub.empty() && sub.size() == calledFunc->genericParams.size()) {
                        Specialization* innerSpec =
                            monomorphizer->requestSpecialization(calledFunc, sub);
                        if (innerSpec) {
                            call->specializedMangledName = innerSpec->mangledName;
                            // Recurse into the newly specialized body
                            analyzeSpecializedBody(innerSpec->funcDecl, sub);
                        }
                    }
                }
            }
            // Walk call arguments
            for (const auto& arg : call->arguments) {
                walkNode(arg.get());
            }
            return;
        }

        // Walk children for common node types
        switch (node->type) {
            case ASTNode::NodeType::BLOCK: {
                BlockStmt* block = static_cast<BlockStmt*>(node);
                for (const auto& s : block->statements) walkNode(s.get());
                break;
            }
            case ASTNode::NodeType::VAR_DECL: {
                VarDeclStmt* vd = static_cast<VarDeclStmt*>(node);
                if (vd->initializer) walkNode(vd->initializer.get());
                break;
            }
            case ASTNode::NodeType::EXPRESSION_STMT: {
                ExpressionStmt* es = static_cast<ExpressionStmt*>(node);
                if (es->expression) walkNode(es->expression.get());
                break;
            }
            case ASTNode::NodeType::IF: {
                IfStmt* ifs = static_cast<IfStmt*>(node);
                if (ifs->condition) walkNode(ifs->condition.get());
                if (ifs->thenBranch) walkNode(ifs->thenBranch.get());
                if (ifs->elseBranch) walkNode(ifs->elseBranch.get());
                break;
            }
            case ASTNode::NodeType::WHILE: {
                WhileStmt* ws = static_cast<WhileStmt*>(node);
                if (ws->condition) walkNode(ws->condition.get());
                if (ws->body) walkNode(ws->body.get());
                break;
            }
            case ASTNode::NodeType::FOR: {
                ForStmt* fs = static_cast<ForStmt*>(node);
                if (fs->initializer) walkNode(fs->initializer.get());
                if (fs->condition) walkNode(fs->condition.get());
                if (fs->update) walkNode(fs->update.get());
                if (fs->body) walkNode(fs->body.get());
                break;
            }
            case ASTNode::NodeType::RETURN: {
                ReturnStmt* rs = static_cast<ReturnStmt*>(node);
                if (rs->value) walkNode(rs->value.get());
                break;
            }
            case ASTNode::NodeType::PASS: {
                PassStmt* ps = static_cast<PassStmt*>(node);
                if (ps->value) walkNode(ps->value.get());
                break;
            }
            case ASTNode::NodeType::FAIL: {
                FailStmt* fs = static_cast<FailStmt*>(node);
                if (fs->errorCode) walkNode(fs->errorCode.get());
                break;
            }
            case ASTNode::NodeType::BINARY_OP: {
                BinaryExpr* be = static_cast<BinaryExpr*>(node);
                if (be->left) walkNode(be->left.get());
                if (be->right) walkNode(be->right.get());
                break;
            }
            case ASTNode::NodeType::UNARY_OP: {
                UnaryExpr* ue = static_cast<UnaryExpr*>(node);
                if (ue->operand) walkNode(ue->operand.get());
                break;
            }
            case ASTNode::NodeType::ASSIGNMENT: {
                BinaryExpr* ae = static_cast<BinaryExpr*>(node);
                if (ae->left) walkNode(ae->left.get());
                if (ae->right) walkNode(ae->right.get());
                break;
            }
            default:
                break;
        }
    };

    walkNode(specDecl->body.get());
}

void TypeChecker::checkExternStmt(ExternStmt* stmt) {
    if (!stmt) {
        return;
    }
    
    // Register all extern function and variable declarations in symbol table
    // This allows them to be referenced by other code in the same scope
    
    for (const auto& decl : stmt->declarations) {
        if (decl->type == ASTNode::NodeType::FUNC_DECL) {
            // Register extern function
            FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(decl.get());
            
            // Resolve return type
            Type* returnType = resolveTypeNode(funcDecl->returnType.get());
            
            // Resolve parameter types
            std::vector<Type*> paramTypes;
            for (const auto& param : funcDecl->parameters) {
                if (param->type == ASTNode::NodeType::PARAMETER) {
                    ParameterNode* paramNode = static_cast<ParameterNode*>(param.get());
                    Type* paramType = resolveTypeNode(paramNode->typeNode.get());
                    paramTypes.push_back(paramType);
                }
            }
            
            // Create function type (extern functions are never async, but may be variadic)
            Type* funcType = new FunctionType(paramTypes, returnType, false, funcDecl->isVariadic);
            
            // Register in symbol table
            symbolTable->defineSymbol(funcDecl->funcName, SymbolKind::FUNCTION, funcType);
        }
        else if (decl->type == ASTNode::NodeType::VAR_DECL) {
            // Register extern variable
            VarDeclStmt* varDecl = static_cast<VarDeclStmt*>(decl.get());
            
            // Resolve variable type
            Type* varType = nullptr;
            if (varDecl->typeNode) {
                varType = resolveTypeNode(varDecl->typeNode.get());
            } else if (!varDecl->typeName.empty()) {
                // Legacy simple type name
                varType = typeSystem->getPrimitiveType(varDecl->typeName);
                if (!varType || varType->getKind() == TypeKind::ERROR) {
                    varType = typeSystem->getStructType(varDecl->typeName);
                }
            }
            
            if (varType && varType->getKind() != TypeKind::ERROR) {
                symbolTable->defineSymbol(varDecl->varName, SymbolKind::VARIABLE, varType);
            }
        }
        else if (decl->type == ASTNode::NodeType::OPAQUE_STRUCT) {
            // Register opaque struct type
            OpaqueStructDecl* opaqueDecl = static_cast<OpaqueStructDecl*>(decl.get());
            
            // Create opaque struct type (empty struct as placeholder)
            typeSystem->createStructType(opaqueDecl->structName, {});
            
            // Mark as opaque in symbol table (type is already registered in typeSystem)
            // The type will be treated as a pointer type in IR generation
        }
    }
}

// ============================================================================
// Module Symbol Importing (Phase 3 from research_module_loading_system)
// ============================================================================

void TypeChecker::importWildcardSymbols(LoadedModule* module, UseStmt* /*stmt*/) {
    if (!module || !module->moduleInfo) {
        return;
    }
    
    // Get the module's export table via public getter
    const auto& exports = module->moduleInfo->getExports();
    
    // Import all PUBLIC symbols into current scope
    for (const auto& [name, exportEntry] : exports) {
        if (exportEntry.visibility != Visibility::PUBLIC) {
            continue;  // Skip non-public symbols
        }
        
        // If a symbol with this name already exists (e.g. registered by the pre-pass
        // with a stale type before structs were resolved), UPDATE it with the correct
        // type from the export table. The MAIN PASS always produces authoritative types.
        if (symbolTable->isDefined(name)) {
            Symbol* existing = symbolTable->lookupSymbol(name);
            if (existing) {
                existing->type = exportEntry.symbol->type;
                if (exportEntry.symbol->funcDecl) {
                    existing->funcDecl = exportEntry.symbol->funcDecl;
                }
            }
            continue;
        }
        
        // Add symbol to current scope
        symbolTable->defineSymbol(name, exportEntry.symbol->kind, exportEntry.symbol->type);
    }
}

void TypeChecker::importSelectiveSymbols(LoadedModule* module, UseStmt* stmt) {
    if (!module || !module->moduleInfo) {
        return;
    }
    
    // Get the module's export table via public getter
    const auto& exports = module->moduleInfo->getExports();
    
    // Import each specified symbol
    // Note: items is a vector<string>, so we just import each name
    for (const std::string& symbolName : stmt->items) {
        // Check if symbol exists in module
        auto it = exports.find(symbolName);
        if (it == exports.end()) {
            addError("Symbol '" + symbolName + "' not found in module", stmt);
            continue;
        }
        
        const auto& exportEntry = it->second;
        
        // Check visibility
        if (exportEntry.visibility != Visibility::PUBLIC) {
            addError("Symbol '" + symbolName + "' is not public and cannot be imported", stmt);
            continue;
        }
        
        // Check for name collision
        if (symbolTable->isDefined(symbolName)) {
            addError("Symbol '" + symbolName + "' already defined in current scope", stmt);
            continue;
        }
        
        // Add symbol to current scope
        symbolTable->defineSymbol(symbolName, exportEntry.symbol->kind, exportEntry.symbol->type);
    }
}

void TypeChecker::importModuleNamespace(LoadedModule* module, UseStmt* stmt, const std::string& modulePath) {
    if (!module || !module->moduleInfo) {
        return;
    }
    
    // Determine namespace name (last component of module path or alias)
    std::string namespaceName;
    if (!stmt->alias.empty()) {
        namespaceName = stmt->alias;
    } else if (stmt->isFilePath) {
        // For file paths, extract filename without extension
        // e.g., "./helper.aria" -> "helper", "../utils/math.aria" -> "math"
        std::string filePath = module->moduleInfo->getPath();
        size_t lastSlash = filePath.find_last_of("/\\");
        std::string filename = (lastSlash != std::string::npos) ? 
                               filePath.substr(lastSlash + 1) : filePath;
        
        // Remove .aria extension
        size_t lastDot = filename.find_last_of('.');
        namespaceName = (lastDot != std::string::npos) ? 
                        filename.substr(0, lastDot) : filename;
    } else {
        // For logical paths, extract last component (e.g., "std.io" -> "io")
        size_t lastDot = modulePath.find_last_of('.');
        if (lastDot != std::string::npos) {
            namespaceName = modulePath.substr(lastDot + 1);
        } else {
            namespaceName = modulePath;
        }
    }
    
    // Check for namespace collision
    if (symbolTable->isDefined(namespaceName)) {
        addError("Namespace '" + namespaceName + "' already defined in current scope", stmt);
        return;
    }
    
    // Create a MODULE symbol with void type as placeholder
    Type* namespaceType = typeSystem->getPrimitiveType("void");
    Symbol* moduleSym = symbolTable->defineSymbol(namespaceName, SymbolKind::MODULE, namespaceType);
    
    // Store the module reference in the symbol for later resolution
    if (moduleSym) {
        moduleSym->setModuleRef(module->moduleInfo.get());
        loadedModules[namespaceName] = module;  // Track loaded module
    }
}

// ============================================================================
// Advanced Loop Type Checking
// ============================================================================

void TypeChecker::checkTillStmt(TillStmt* stmt) {
    // Check limit expression - must be integer type
    Type* limitType = inferType(stmt->limit.get());
    if (limitType->getKind() != TypeKind::ERROR) {
        PrimitiveType* limitPrim = dynamic_cast<PrimitiveType*>(limitType);
        if (!limitPrim || (limitPrim->getName().find("int") == std::string::npos && 
                           limitPrim->getName().find("uint") == std::string::npos)) {
            addError("till limit must be integer type, got '" + limitType->toString() + "'", 
                    stmt->limit.get());
        }
    }
    
    // Check step expression - must be integer type
    Type* stepType = inferType(stmt->step.get());
    if (stepType->getKind() != TypeKind::ERROR) {
        PrimitiveType* stepPrim = dynamic_cast<PrimitiveType*>(stepType);
        if (!stepPrim || (stepPrim->getName().find("int") == std::string::npos && 
                          stepPrim->getName().find("uint") == std::string::npos)) {
            addError("till step must be integer type, got '" + stepType->toString() + "'", 
                    stmt->step.get());
        }
    }
    
    // Check body
    checkStatement(stmt->body.get());
}

void TypeChecker::checkLoopStmt(LoopStmt* stmt) {
    // Check start expression - must be integer type
    Type* startType = inferType(stmt->start.get());
    if (startType->getKind() != TypeKind::ERROR) {
        PrimitiveType* startPrim = dynamic_cast<PrimitiveType*>(startType);
        if (!startPrim || (startPrim->getName().find("int") == std::string::npos && 
                           startPrim->getName().find("uint") == std::string::npos)) {
            addError("loop start must be integer type, got '" + startType->toString() + "'", 
                    stmt->start.get());
        }
    }
    
    // Check limit expression - must be integer type
    Type* limitType = inferType(stmt->limit.get());
    if (limitType->getKind() != TypeKind::ERROR) {
        PrimitiveType* limitPrim = dynamic_cast<PrimitiveType*>(limitType);
        if (!limitPrim || (limitPrim->getName().find("int") == std::string::npos && 
                           limitPrim->getName().find("uint") == std::string::npos)) {
            addError("loop limit must be integer type, got '" + limitType->toString() + "'", 
                    stmt->limit.get());
        }
    }
    
    // Check step expression - must be integer type
    Type* stepType = inferType(stmt->step.get());
    if (stepType->getKind() != TypeKind::ERROR) {
        PrimitiveType* stepPrim = dynamic_cast<PrimitiveType*>(stepType);
        if (!stepPrim || (stepPrim->getName().find("int") == std::string::npos && 
                          stepPrim->getName().find("uint") == std::string::npos)) {
            addError("loop step must be integer type, got '" + stepType->toString() + "'", 
                    stmt->step.get());
        }
    }
    
    // Check body
    checkStatement(stmt->body.get());
}

void TypeChecker::checkWhenStmt(WhenStmt* stmt) {
    // Check condition - must be boolean type
    Type* condType = inferType(stmt->condition.get());
    if (condType->getKind() != TypeKind::ERROR) {
        PrimitiveType* condPrim = dynamic_cast<PrimitiveType*>(condType);
        if (!condPrim || condPrim->getName() != "bool") {
            addError("when condition must be 'bool' type, got '" + condType->toString() + "'", 
                    stmt->condition.get());
        }
    }
    
    // Check body
    checkStatement(stmt->body.get());
    
    // Check then block if present
    if (stmt->then_block) {
        checkStatement(stmt->then_block.get());
    }
    
    // Check end block if present
    if (stmt->end_block) {
        checkStatement(stmt->end_block.get());
    }
}

void TypeChecker::checkPickStmt(PickStmt* stmt) {
    // Infer the selector type
    Type* selectorType = inferType(stmt->selector.get());
    
    // Only perform exhaustiveness checking if selector type is valid
    if (selectorType->getKind() != TypeKind::ERROR) {
        // v0.5.3: If the selector is a limit<> variable, defer exhaustiveness
        // checking to the SMT-based Phase 2b (provePickExhaustiveness).
        // The Rules constraint bounds the domain, so infinite-domain rejection
        // does not apply.
        bool selectorHasRules = false;
        if (stmt->selector && stmt->selector->type == ASTNode::NodeType::IDENTIFIER) {
            auto* ident = static_cast<IdentifierExpr*>(stmt->selector.get());
            if (limitedVariables.count(ident->name)) {
                selectorHasRules = true;
            }
        }

        if (!selectorHasRules) {
            // Check exhaustiveness
            ExhaustivenessAnalyzer::Analysis result = ExhaustivenessAnalyzer::analyze(stmt, selectorType);
            
            // Report error if not exhaustive
            if (!result.isExhaustive) {
                addError(result.errorMessage, stmt);
            }
        }
    }
    
    // Phase 3.5: Result enforcement for pick statements
    // Save current state before pick
    ResultCheckState beforePick = currentResultState;
    std::vector<ResultCheckState> caseStates;
    
    // Type-check all case bodies with Result knowledge
    for (const auto& caseNode : stmt->cases) {
        PickCase* pickCase = static_cast<PickCase*>(caseNode.get());
        
        // Start with state from before pick
        ResultCheckState caseState = beforePick;
        
        // Check if selector is .is_error field access on a Result variable
        // If so, and pattern is a boolean literal, propagate knowledge
        if (stmt->selector->type == ASTNode::NodeType::MEMBER_ACCESS) {
            MemberAccessExpr* memberAccess = static_cast<MemberAccessExpr*>(stmt->selector.get());
            if (memberAccess->member == "is_error") {
                // Get the Result variable name
                std::string resultVar;
                if (memberAccess->object->type == ASTNode::NodeType::IDENTIFIER) {
                    IdentifierExpr* objIdent = static_cast<IdentifierExpr*>(memberAccess->object.get());
                    resultVar = objIdent->name;
                }
                
                // Check if pattern is a boolean literal
                if (!resultVar.empty() && pickCase->pattern->type == ASTNode::NodeType::LITERAL) {
                    LiteralExpr* patternLit = static_cast<LiteralExpr*>(pickCase->pattern.get());
                    if (std::holds_alternative<bool>(patternLit->value)) {
                        bool isError = std::get<bool>(patternLit->value);
                        
                        if (isError) {
                            // Case matches when is_error == true
                            caseState.markKnownError(resultVar);
                        } else {
                            // Case matches when is_error == false
                            caseState.markKnownSuccess(resultVar);
                        }
                    }
                }
            }
        }
        
        // Check case body with derived knowledge
        currentResultState = caseState;
        if (pickCase->body) {
            checkStatement(pickCase->body.get());
        }
        
        // Save state after this case
        caseStates.push_back(currentResultState);
    }
    
    // After pick: merge all case states conservatively
    if (!caseStates.empty()) {
        currentResultState = caseStates[0];
        for (size_t i = 1; i < caseStates.size(); i++) {
            currentResultState = ResultCheckState::merge(currentResultState, caseStates[i]);
        }
    } else {
        // No cases - keep before state
        currentResultState = beforePick;
    }
}

// ============================================================================
// Result<T> "No Checky No Val" Enforcement
// ============================================================================

/**
 * Mark a Result variable as checked
 * 
 * Called when code accesses .is_error on a Result variable.
 * This allows subsequent .value or .error access on the same variable.
 * 
 * Phase 2: Uses ResultCheckState for control flow tracking
 */
void TypeChecker::markResultChecked(const std::string& varName) {
    if (!varName.empty()) {
        currentResultState.markChecked(varName);
    }
}

/**
 * Check if a Result variable has been checked
 * 
 * Returns true if .is_error was accessed on this variable.
 */
bool TypeChecker::isResultChecked(const std::string& varName) const {
    return currentResultState.isChecked(varName);
}

/**
 * Get variable name from an expression node
 * 
 * Used for tracking Result variables in "no checky no val" enforcement.
 * 
 * Handles:
 * - IDENTIFIER: Simple variable (r.is_error)
 * - MEMBER_ACCESS: Struct field (obj.result_field.is_error)
 * 
 * Returns empty string for complex expressions (function calls, etc.)
 * These bypass tracking for now - Phase 2 will handle more cases.
 */
std::string TypeChecker::getVariableName(ASTNode* expr) const {
    if (!expr) return "";
    
    if (expr->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr);
        return ident->name;
    }
    
    if (expr->type == ASTNode::NodeType::MEMBER_ACCESS) {
        MemberAccessExpr* member = static_cast<MemberAccessExpr*>(expr);
        std::string baseName = getVariableName(member->object.get());
        if (!baseName.empty()) {
            return baseName + "." + member->member;
        }
    }
    
    // Complex expression - can't track for now
    return "";
}

// ============================================================================
// Phase 2: Control Flow Analysis for Result Checking
// ============================================================================

/**
 * Analyze an if condition to extract Result check knowledge
 * 
 * Detects patterns like:
 * - r.is_error == true  → then knows ERROR, else knows SUCCESS
 * - r.is_error == false → then knows SUCCESS, else knows ERROR
 * - r.is_error != true  → then knows SUCCESS, else knows ERROR
 * - !r.is_error         → then knows SUCCESS, else knows ERROR
 * - !(r.is_error)       → then knows SUCCESS, else knows ERROR
 * 
 * Updates thenState and elseState with what each branch knows
 */
void TypeChecker::analyzeConditionForResultChecks(
    ASTNode* condition,
    ResultCheckState& thenState,
    ResultCheckState& elseState
) {
    using namespace frontend;
    
    // Handle negation: !condition
    if (condition->type == ASTNode::NodeType::UNARY_OP) {
        UnaryExpr* unaryExpr = static_cast<UnaryExpr*>(condition);
        
        // Only handle logical NOT
        if (unaryExpr->op.type == TokenType::TOKEN_BANG) {
            // Recursively analyze the negated condition
            // But flip then/else since negation inverts the condition
            analyzeConditionForResultChecks(unaryExpr->operand.get(), elseState, thenState);
            return;
        }
    }
    
    // Handle direct boolean member access: if (r.is_error) { ... }
    if (condition->type == ASTNode::NodeType::MEMBER_ACCESS) {
        MemberAccessExpr* memberAccess = static_cast<MemberAccessExpr*>(condition);
        
        // Check if member is .is_error
        if (memberAccess->member == "is_error") {
            std::string varName = getVariableName(memberAccess->object.get());
            if (!varName.empty()) {
                // if (r.is_error) → then knows ERROR, else knows SUCCESS
                thenState.markKnownError(varName);
                elseState.markKnownSuccess(varName);
                return;
            }
        }
    }
    
    // Only analyze binary comparison expressions
    if (condition->type != ASTNode::NodeType::BINARY_OP) {
        return;
    }
    
    BinaryExpr* binExpr = static_cast<BinaryExpr*>(condition);
    
    // Only analyze == and != comparisons
    if (binExpr->op.type != TokenType::TOKEN_EQUAL_EQUAL &&
        binExpr->op.type != TokenType::TOKEN_BANG_EQUAL) {
        return;
    }
    
    // Check if one side is .is_error member access, other is boolean literal
    MemberAccessExpr* memberAccess = nullptr;
    LiteralExpr* literal = nullptr;
    
    // Check left.is_error == right
    if (binExpr->left->type == ASTNode::NodeType::MEMBER_ACCESS &&
        binExpr->right->type == ASTNode::NodeType::LITERAL) {
        memberAccess = static_cast<MemberAccessExpr*>(binExpr->left.get());
        literal = static_cast<LiteralExpr*>(binExpr->right.get());
    }
    // Check left == right.is_error
    else if (binExpr->right->type == ASTNode::NodeType::MEMBER_ACCESS &&
             binExpr->left->type == ASTNode::NodeType::LITERAL) {
        memberAccess = static_cast<MemberAccessExpr*>(binExpr->right.get());
        literal = static_cast<LiteralExpr*>(binExpr->left.get());
    }
    
    // Not a pattern we recognize
    if (!memberAccess || !literal) {
        return;
    }
    
    // Check if member is .is_error
    if (memberAccess->member != "is_error") {
        return;
    }
    
    // Check if literal is boolean
    if (!std::holds_alternative<bool>(literal->value)) {
        return;
    }
    
    // Get the Result variable name
    std::string varName = getVariableName(memberAccess->object.get());
    if (varName.empty()) {
        return;
    }
    
    // Get the boolean value being compared
    bool errorValue = std::get<bool>(literal->value);
    
    // Determine what each branch knows based on comparison
    bool comparing_equal = (binExpr->op.type == TokenType::TOKEN_EQUAL_EQUAL);
    
    if (comparing_equal) {
        // r.is_error == true or r.is_error == false
        if (errorValue) {
            // r.is_error == true
            thenState.markKnownError(varName);
            elseState.markKnownSuccess(varName);
        } else {
            // r.is_error == false
            thenState.markKnownSuccess(varName);
            elseState.markKnownError(varName);
        }
    } else {
        // r.is_error != true or r.is_error != false
        if (errorValue) {
            // r.is_error != true (means is_error == false)
            thenState.markKnownSuccess(varName);
            elseState.markKnownError(varName);
        } else {
            // r.is_error != false (means is_error == true)
            thenState.markKnownError(varName);
            elseState.markKnownSuccess(varName);
        }
    }
}

/**
 * Check if a branch always returns/exits
 * 
 * Used for early return analysis:
 *   if (r.is_error) { return; }
 *   // After this, we know r.is_error == false
 * 
 * Also detects break/continue for loop analysis:
 *   while (true) {
 *     if (r.is_error) break;
 *     // After: knows !r.is_error
 *   }
 * 
 * Phase 2: Simple check for return at end of block
 * Phase 3: Also checks for break/continue
 */
bool TypeChecker::branchAlwaysReturns(ASTNode* branch) {
    if (!branch) {
        return false;
    }
    
    // Helper lambda to check if a single statement is a terminator
    auto isTerminator = [](ASTNode* node) -> bool {
        if (!node) return false;
        
        // Direct terminator node types
        if (node->type == ASTNode::NodeType::RETURN ||
            node->type == ASTNode::NodeType::PASS ||
            node->type == ASTNode::NodeType::FAIL ||
            node->type == ASTNode::NodeType::BREAK ||
            node->type == ASTNode::NodeType::CONTINUE) {
            return true;
        }
        
        // exit is parsed as ExpressionStmt(CallExpr("exit", [code]))
        if (node->type == ASTNode::NodeType::EXPRESSION_STMT) {
            auto* exprStmt = static_cast<ExpressionStmt*>(node);
            if (exprStmt->expression && 
                exprStmt->expression->type == ASTNode::NodeType::CALL) {
                auto* call = static_cast<CallExpr*>(exprStmt->expression.get());
                if (call->callee && call->callee->type == ASTNode::NodeType::IDENTIFIER) {
                    auto* ident = static_cast<IdentifierExpr*>(call->callee.get());
                    if (ident->name == "exit") {
                        return true;
                    }
                }
            }
        }
        
        return false;
    };
    
    // Check block statements
    if (branch->type == ASTNode::NodeType::BLOCK) {
        BlockStmt* block = static_cast<BlockStmt*>(branch);
        if (block->statements.empty()) {
            return false;
        }
        
        // Check if last statement is a terminator
        return isTerminator(block->statements.back().get());
    }
    
    // Single statement (no block)
    return isTerminator(branch);
}

// ============================================================================
// Program Validation
// ============================================================================

bool TypeChecker::validateFailsafeExists() {
    // Check if failsafe() function is defined in the symbol table
    Symbol* failsafeSymbol = symbolTable->resolveSymbol("failsafe");
    
    if (!failsafeSymbol) {
        addError("Every Aria program must define a 'func:failsafe = int32(tbb32:err)' function. "
                 "failsafe is one of two program endpoints (with main). "
                 "It handles error exits via 'exit <code>' where code > 0.", nullptr);
        return false;
    }
    
    // Validate that it's actually a function
    if (failsafeSymbol->type->getKind() != TypeKind::FUNCTION) {
        addError("'failsafe' must be a function, but is defined as: " + 
                 failsafeSymbol->type->toString(), nullptr);
        return false;
    }
    
    // Signature details validated in checkFuncDecl() when the function is parsed
    
    return true;
}

bool TypeChecker::validateMainExists() {
    // Check if main() function is defined in the symbol table
    Symbol* mainSymbol = symbolTable->resolveSymbol("main");
    
    if (!mainSymbol) {
        addError("Every Aria program must define a 'func:main = int32()' function. "
                 "main is one of two program endpoints (with failsafe). "
                 "It is the entry point and must call 'exit <code>' where code 0 = success.", nullptr);
        return false;
    }
    
    // Validate that it's actually a function
    if (mainSymbol->type->getKind() != TypeKind::FUNCTION) {
        addError("'main' must be a function, but is defined as: " + 
                 mainSymbol->type->toString(), nullptr);
        return false;
    }
    
    // Signature details validated in checkFuncDecl() when the function is parsed
    
    return true;
}

// ============================================================================
// v0.8.3: Macro System — AST-Level Macros, Derive, Attributes
// ============================================================================

void TypeChecker::checkMacroDecl(MacroDeclStmt* stmt) {
    if (!stmt) return;
    
    // Check for duplicate macro names
    if (macroRegistry.count(stmt->macroName)) {
        addError("Macro '" + stmt->macroName + "' is already defined", stmt);
        return;
    }
    
    // Register the macro for later expansion
    macroRegistry[stmt->macroName] = stmt;
}

Type* TypeChecker::inferMacroInvocation(MacroInvocationExpr* expr) {
    if (!expr) return typeSystem->getErrorType();
    
    // Look up the macro
    auto it = macroRegistry.find(expr->macroName);
    if (it == macroRegistry.end()) {
        addError("Undefined macro '" + expr->macroName + "'", expr);
        return typeSystem->getErrorType();
    }
    
    MacroDeclStmt* macroDef = it->second;
    
    // Check argument count
    if (expr->arguments.size() != macroDef->paramNames.size()) {
        addError("Macro '" + expr->macroName + "' expects " + 
                 std::to_string(macroDef->paramNames.size()) + " arguments, got " +
                 std::to_string(expr->arguments.size()), expr);
        return typeSystem->getErrorType();
    }
    
    // Build substitution map: param name -> argument AST
    std::map<std::string, ASTNodePtr> substitutions;
    for (size_t i = 0; i < macroDef->paramNames.size(); ++i) {
        substitutions[macroDef->paramNames[i]] = expr->arguments[i];
    }
    
    // Clone the macro body with parameter substitutions
    ASTNodePtr expanded = cloneAST(macroDef->body.get(), substitutions);
    expr->expandedAST = expanded;
    
    // Type-check the expanded AST
    if (expanded) {
        // If the expanded AST is a block, check it and return the type of the last expression
        if (expanded->type == ASTNode::NodeType::BLOCK) {
            auto* block = static_cast<BlockStmt*>(expanded.get());
            Type* lastType = typeSystem->getPrimitiveType("void");
            for (const auto& s : block->statements) {
                if (s->type == ASTNode::NodeType::EXPRESSION_STMT) {
                    auto* exprStmt = static_cast<ExpressionStmt*>(s.get());
                    lastType = inferType(exprStmt->expression.get());
                } else if (s->isExpression()) {
                    lastType = inferType(s.get());
                } else {
                    checkStatement(s.get());
                }
            }
            return lastType;
        }
        // If it's an expression, infer its type
        if (expanded->isExpression()) {
            return inferType(expanded.get());
        }
        // Statement — check it and return void
        checkStatement(expanded.get());
    }
    
    return typeSystem->getPrimitiveType("void");
}

ASTNodePtr TypeChecker::cloneAST(ASTNode* node, const std::map<std::string, ASTNodePtr>& substitutions) {
    if (!node) return nullptr;
    
    // If this is an identifier that matches a macro parameter, substitute it
    if (node->type == ASTNode::NodeType::IDENTIFIER) {
        auto* ident = static_cast<IdentifierExpr*>(node);
        auto it = substitutions.find(ident->name);
        if (it != substitutions.end()) {
            return it->second;  // Return the argument AST directly
        }
        // Not a macro parameter — return a copy of the identifier
        return std::make_shared<IdentifierExpr>(ident->name, ident->line, ident->column);
    }
    
    // For binary operations, clone both sides
    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* bin = static_cast<BinaryExpr*>(node);
        auto newLeft = cloneAST(bin->left.get(), substitutions);
        auto newRight = cloneAST(bin->right.get(), substitutions);
        return std::make_shared<BinaryExpr>(newLeft, bin->op, newRight, bin->line, bin->column);
    }
    
    // Unary operations
    if (node->type == ASTNode::NodeType::UNARY_OP) {
        auto* un = static_cast<UnaryExpr*>(node);
        auto newOperand = cloneAST(un->operand.get(), substitutions);
        return std::make_shared<UnaryExpr>(un->op, newOperand, un->isPostfix, un->line, un->column);
    }
    
    // Call expressions
    if (node->type == ASTNode::NodeType::CALL) {
        auto* call = static_cast<CallExpr*>(node);
        auto newCallee = cloneAST(call->callee.get(), substitutions);
        std::vector<ASTNodePtr> newArgs;
        for (const auto& arg : call->arguments) {
            newArgs.push_back(cloneAST(arg.get(), substitutions));
        }
        return std::make_shared<CallExpr>(newCallee, newArgs, call->line, call->column);
    }
    
    // Literals — just copy
    if (node->type == ASTNode::NodeType::LITERAL) {
        auto* lit = static_cast<LiteralExpr*>(node);
        auto clone = std::visit([&](auto&& val) {
            return std::make_shared<LiteralExpr>(val, lit->line, lit->column);
        }, lit->value);
        clone->raw_value_string = lit->raw_value_string;
        clone->explicit_type = lit->explicit_type;
        return clone;
    }
    
    // Block statements
    if (node->type == ASTNode::NodeType::BLOCK) {
        auto* block = static_cast<BlockStmt*>(node);
        std::vector<ASTNodePtr> newStmts;
        for (const auto& s : block->statements) {
            auto cloned = cloneAST(s.get(), substitutions);
            if (cloned) newStmts.push_back(cloned);
        }
        return std::make_shared<BlockStmt>(newStmts, block->line, block->column);
    }
    
    // Expression statements
    if (node->type == ASTNode::NodeType::EXPRESSION_STMT) {
        auto* exprStmt = static_cast<ExpressionStmt*>(node);
        auto newExpr = cloneAST(exprStmt->expression.get(), substitutions);
        return std::make_shared<ExpressionStmt>(newExpr, exprStmt->line, exprStmt->column);
    }
    
    // Return/Pass/Fail
    if (node->type == ASTNode::NodeType::RETURN) {
        auto* ret = static_cast<ReturnStmt*>(node);
        auto newVal = ret->value ? cloneAST(ret->value.get(), substitutions) : nullptr;
        return std::make_shared<ReturnStmt>(newVal, ret->line, ret->column);
    }
    
    if (node->type == ASTNode::NodeType::PASS) {
        auto* pass = static_cast<PassStmt*>(node);
        auto newVal = cloneAST(pass->value.get(), substitutions);
        return std::make_shared<PassStmt>(newVal, pass->line, pass->column);
    }
    
    // Variable declarations
    if (node->type == ASTNode::NodeType::VAR_DECL) {
        auto* var = static_cast<VarDeclStmt*>(node);
        auto newInit = var->initializer ? cloneAST(var->initializer.get(), substitutions) : nullptr;
        auto newTypeNode = var->typeNode ? cloneAST(var->typeNode.get(), substitutions) : nullptr;
        auto clone = std::make_shared<VarDeclStmt>(newTypeNode, var->varName, newInit, var->line, var->column);
        clone->typeName = var->typeName;
        clone->isWild = var->isWild;
        clone->isConst = var->isConst;
        return clone;
    }
    
    // If statement
    if (node->type == ASTNode::NodeType::IF) {
        auto* ifStmt = static_cast<IfStmt*>(node);
        auto newCond = cloneAST(ifStmt->condition.get(), substitutions);
        auto newThen = cloneAST(ifStmt->thenBranch.get(), substitutions);
        auto newElse = ifStmt->elseBranch ? cloneAST(ifStmt->elseBranch.get(), substitutions) : nullptr;
        return std::make_shared<IfStmt>(newCond, newThen, newElse, ifStmt->line, ifStmt->column);
    }
    
    // Member access
    if (node->type == ASTNode::NodeType::MEMBER_ACCESS) {
        auto* mem = static_cast<MemberAccessExpr*>(node);
        auto newObj = cloneAST(mem->object.get(), substitutions);
        return std::make_shared<MemberAccessExpr>(newObj, mem->member, mem->isPointerAccess, mem->isSafeNavigation, mem->line, mem->column);
    }
    
    // Type annotations — pass through unchanged
    if (node->type == ASTNode::NodeType::TYPE_ANNOTATION) {
        auto* ta = static_cast<SimpleType*>(node);
        return std::make_shared<SimpleType>(ta->typeName, ta->line, ta->column);
    }
    
    // Fallback: return nullptr (node type not supported for cloning)
    // This is safe — the type checker will report an appropriate error
    return nullptr;
}

void TypeChecker::expandDeriveAttributes(StructDeclStmt* stmt) {
    // Auto-register built-in derive traits if not yet declared
    static const std::vector<std::string> builtinDeriveTraits = {
        "ToString", "Eq", "Hash", "Clone", "Debug", "Ord"
    };
    for (const auto& bt : builtinDeriveTraits) {
        if (!symbolTable->lookupSymbol(bt)) {
            // Create a synthetic empty trait declaration
            auto syntheticTrait = std::make_shared<TraitDeclStmt>(
                bt, std::vector<TraitMethod>{}, stmt->line, stmt->column);
            syntheticNodes.push_back(syntheticTrait);
            checkTraitDecl(static_cast<TraitDeclStmt*>(syntheticTrait.get()));
        }
    }

    for (const auto& attr : stmt->attributes) {
        if (attr.name != "derive") continue;
        
        for (const auto& traitName : attr.args) {
            // Generate synthetic impl for each derived trait
            
            if (traitName == "ToString") {
                // Generate: impl:ToString:for:StructName = {
                //   func:to_string = string(StructName:self) {
                //     pass(`StructName { field1: &{self.field1}, field2: &{self.field2} }`);
                //   };
                // };
                
                // Build a simple to_string function that returns the struct name
                // with fields as a template literal
                auto selfParam = std::make_shared<ParameterNode>(
                    std::make_shared<SimpleType>(stmt->structName, stmt->line, stmt->column),
                    "self", nullptr, stmt->line, stmt->column);
                
                // Build the string representation: "StructName { ... }"
                std::string formatStr = stmt->structName + " { ";
                for (size_t i = 0; i < stmt->fields.size(); ++i) {
                    auto* field = static_cast<VarDeclStmt*>(stmt->fields[i].get());
                    if (i > 0) formatStr += ", ";
                    formatStr += field->varName + ": %";  // placeholder
                }
                formatStr += " }";
                
                // For now, generate a simple string literal return
                auto strLit = std::make_shared<LiteralExpr>(
                    formatStr, stmt->line, stmt->column);
                auto passStmt = std::make_shared<PassStmt>(strLit, stmt->line, stmt->column);
                
                std::vector<ASTNodePtr> bodyStmts = {passStmt};
                auto body = std::make_shared<BlockStmt>(bodyStmts, stmt->line, stmt->column);
                auto retType = std::make_shared<SimpleType>("string", stmt->line, stmt->column);
                
                auto func = std::make_shared<FuncDeclStmt>(
                    "to_string", retType,
                    std::vector<ASTNodePtr>{selfParam},
                    body, stmt->line, stmt->column);
                
                std::vector<ASTNodePtr> methods = {func};
                auto impl = std::make_shared<ImplDeclStmt>(
                    "ToString", stmt->structName, std::move(methods),
                    stmt->line, stmt->column);
                
                // Store and process the synthetic impl
                syntheticNodes.push_back(impl);
                checkImplDecl(static_cast<ImplDeclStmt*>(impl.get()));
                
            } else if (traitName == "Eq") {
                // Generate: impl:Eq:for:StructName = {
                //   func:eq = bool(StructName:self, StructName:other) { ... };
                // };
                
                auto selfParam = std::make_shared<ParameterNode>(
                    std::make_shared<SimpleType>(stmt->structName, stmt->line, stmt->column),
                    "self", nullptr, stmt->line, stmt->column);
                auto otherParam = std::make_shared<ParameterNode>(
                    std::make_shared<SimpleType>(stmt->structName, stmt->line, stmt->column),
                    "other", nullptr, stmt->line, stmt->column);
                
                // Compare all fields: self.field1 == other.field1 && self.field2 == other.field2 && ...
                ASTNodePtr comparison = nullptr;
                for (const auto& field : stmt->fields) {
                    auto* fieldDecl = static_cast<VarDeclStmt*>(field.get());
                    auto selfAccess = std::make_shared<MemberAccessExpr>(
                        std::make_shared<IdentifierExpr>("self", stmt->line, stmt->column),
                        fieldDecl->varName, false, false, stmt->line, stmt->column);
                    auto otherAccess = std::make_shared<MemberAccessExpr>(
                        std::make_shared<IdentifierExpr>("other", stmt->line, stmt->column),
                        fieldDecl->varName, false, false, stmt->line, stmt->column);
                    
                    frontend::Token eqOp(frontend::TokenType::TOKEN_EQUAL_EQUAL, "==", stmt->line, stmt->column);
                    auto fieldCmp = std::make_shared<BinaryExpr>(selfAccess, eqOp, otherAccess, stmt->line, stmt->column);
                    
                    if (!comparison) {
                        comparison = fieldCmp;
                    } else {
                        frontend::Token andOp(frontend::TokenType::TOKEN_AND_AND, "&&", stmt->line, stmt->column);
                        comparison = std::make_shared<BinaryExpr>(comparison, andOp, fieldCmp, stmt->line, stmt->column);
                    }
                }
                
                // If no fields, return true
                if (!comparison) {
                    comparison = std::make_shared<LiteralExpr>(true, stmt->line, stmt->column);
                }
                
                auto passStmt = std::make_shared<PassStmt>(comparison, stmt->line, stmt->column);
                std::vector<ASTNodePtr> bodyStmts = {passStmt};
                auto body = std::make_shared<BlockStmt>(bodyStmts, stmt->line, stmt->column);
                auto retType = std::make_shared<SimpleType>("bool", stmt->line, stmt->column);
                
                auto func = std::make_shared<FuncDeclStmt>(
                    "eq", retType,
                    std::vector<ASTNodePtr>{selfParam, otherParam},
                    body, stmt->line, stmt->column);
                
                std::vector<ASTNodePtr> methods = {func};
                auto impl = std::make_shared<ImplDeclStmt>(
                    "Eq", stmt->structName, std::move(methods),
                    stmt->line, stmt->column);
                
                syntheticNodes.push_back(impl);
                checkImplDecl(static_cast<ImplDeclStmt*>(impl.get()));
                
            } else if (traitName == "Hash") {
                // Generate: impl:Hash:for:StructName = {
                //   func:hash = uint64(StructName:self) { ... };
                // };
                
                auto selfParam = std::make_shared<ParameterNode>(
                    std::make_shared<SimpleType>(stmt->structName, stmt->line, stmt->column),
                    "self", nullptr, stmt->line, stmt->column);
                
                // Simple FNV-1a hash combining all fields
                // For now, return a constant placeholder — real impl needs field access codegen
                auto hashVal = std::make_shared<LiteralExpr>(
                    (int64_t)14695981039346656037ULL, stmt->line, stmt->column);
                hashVal->raw_value_string = "14695981039346656037u64";
                hashVal->explicit_type = "uint64";
                auto passStmt = std::make_shared<PassStmt>(hashVal, stmt->line, stmt->column);
                
                std::vector<ASTNodePtr> bodyStmts = {passStmt};
                auto body = std::make_shared<BlockStmt>(bodyStmts, stmt->line, stmt->column);
                auto retType = std::make_shared<SimpleType>("uint64", stmt->line, stmt->column);
                
                auto func = std::make_shared<FuncDeclStmt>(
                    "hash", retType,
                    std::vector<ASTNodePtr>{selfParam},
                    body, stmt->line, stmt->column);
                
                std::vector<ASTNodePtr> methods = {func};
                auto impl = std::make_shared<ImplDeclStmt>(
                    "Hash", stmt->structName, std::move(methods),
                    stmt->line, stmt->column);
                
                syntheticNodes.push_back(impl);
                checkImplDecl(static_cast<ImplDeclStmt*>(impl.get()));
                
            } else if (traitName == "Clone") {
                // Generate: impl:Clone:for:StructName = {
                //   func:clone = StructName(StructName:self) { pass(self); };
                // };
                
                auto selfParam = std::make_shared<ParameterNode>(
                    std::make_shared<SimpleType>(stmt->structName, stmt->line, stmt->column),
                    "self", nullptr, stmt->line, stmt->column);
                
                auto selfRef = std::make_shared<IdentifierExpr>("self", stmt->line, stmt->column);
                auto passStmt = std::make_shared<PassStmt>(selfRef, stmt->line, stmt->column);
                
                std::vector<ASTNodePtr> bodyStmts = {passStmt};
                auto body = std::make_shared<BlockStmt>(bodyStmts, stmt->line, stmt->column);
                auto retType = std::make_shared<SimpleType>(stmt->structName, stmt->line, stmt->column);
                
                auto func = std::make_shared<FuncDeclStmt>(
                    "clone", retType,
                    std::vector<ASTNodePtr>{selfParam},
                    body, stmt->line, stmt->column);
                
                std::vector<ASTNodePtr> methods = {func};
                auto impl = std::make_shared<ImplDeclStmt>(
                    "Clone", stmt->structName, std::move(methods),
                    stmt->line, stmt->column);
                
                syntheticNodes.push_back(impl);
                checkImplDecl(static_cast<ImplDeclStmt*>(impl.get()));
                
            } else if (traitName == "Debug") {
                // Generate: impl:Debug:for:StructName = {
                //   func:debug = string(StructName:self) { ... };
                // };
                // Similar to ToString but with type annotations for each field
                
                auto selfParam = std::make_shared<ParameterNode>(
                    std::make_shared<SimpleType>(stmt->structName, stmt->line, stmt->column),
                    "self", nullptr, stmt->line, stmt->column);
                
                std::string debugStr = stmt->structName + " { ";
                for (size_t i = 0; i < stmt->fields.size(); ++i) {
                    auto* field = static_cast<VarDeclStmt*>(stmt->fields[i].get());
                    if (i > 0) debugStr += ", ";
                    std::string fieldType = field->typeNode ? field->typeNode->toString() : field->typeName;
                    debugStr += field->varName + ": " + fieldType + "(%)";
                }
                debugStr += " }";
                
                auto strLit = std::make_shared<LiteralExpr>(
                    debugStr, stmt->line, stmt->column);
                auto passStmt = std::make_shared<PassStmt>(strLit, stmt->line, stmt->column);
                
                std::vector<ASTNodePtr> bodyStmts = {passStmt};
                auto body = std::make_shared<BlockStmt>(bodyStmts, stmt->line, stmt->column);
                auto retType = std::make_shared<SimpleType>("string", stmt->line, stmt->column);
                
                auto func = std::make_shared<FuncDeclStmt>(
                    "debug", retType,
                    std::vector<ASTNodePtr>{selfParam},
                    body, stmt->line, stmt->column);
                
                std::vector<ASTNodePtr> methods = {func};
                auto impl = std::make_shared<ImplDeclStmt>(
                    "Debug", stmt->structName, std::move(methods),
                    stmt->line, stmt->column);
                
                syntheticNodes.push_back(impl);
                checkImplDecl(static_cast<ImplDeclStmt*>(impl.get()));
                
            } else if (traitName == "Ord") {
                // Generate: impl:Ord:for:StructName = {
                //   func:less_than = bool(StructName:self, StructName:other) { ... };
                // };
                // Lexicographic field-by-field comparison using <
                
                auto selfParam = std::make_shared<ParameterNode>(
                    std::make_shared<SimpleType>(stmt->structName, stmt->line, stmt->column),
                    "self", nullptr, stmt->line, stmt->column);
                auto otherParam = std::make_shared<ParameterNode>(
                    std::make_shared<SimpleType>(stmt->structName, stmt->line, stmt->column),
                    "other", nullptr, stmt->line, stmt->column);
                
                // Lexicographic ordering: for each field, if self.f < other.f return true,
                // if other.f < self.f return false, else continue to next field.
                // Final: return false (all fields equal).
                std::vector<ASTNodePtr> bodyStmts;
                
                for (const auto& field : stmt->fields) {
                    auto* fieldDecl = static_cast<VarDeclStmt*>(field.get());
                    auto selfAccess = std::make_shared<MemberAccessExpr>(
                        std::make_shared<IdentifierExpr>("self", stmt->line, stmt->column),
                        fieldDecl->varName, false, false, stmt->line, stmt->column);
                    auto otherAccess = std::make_shared<MemberAccessExpr>(
                        std::make_shared<IdentifierExpr>("other", stmt->line, stmt->column),
                        fieldDecl->varName, false, false, stmt->line, stmt->column);
                    
                    // if self.field < other.field { pass(true); };
                    frontend::Token ltOp(frontend::TokenType::TOKEN_LESS, "<", stmt->line, stmt->column);
                    auto ltCmp = std::make_shared<BinaryExpr>(selfAccess, ltOp, otherAccess, stmt->line, stmt->column);
                    
                    auto passTrue = std::make_shared<PassStmt>(
                        std::make_shared<LiteralExpr>(true, stmt->line, stmt->column),
                        stmt->line, stmt->column);
                    auto ltBody = std::make_shared<BlockStmt>(
                        std::vector<ASTNodePtr>{passTrue}, stmt->line, stmt->column);
                    auto ltIf = std::make_shared<IfStmt>(
                        ltCmp, ltBody, nullptr, stmt->line, stmt->column);
                    bodyStmts.push_back(ltIf);
                    
                    // if other.field < self.field { pass(false); };
                    auto selfAccess2 = std::make_shared<MemberAccessExpr>(
                        std::make_shared<IdentifierExpr>("self", stmt->line, stmt->column),
                        fieldDecl->varName, false, false, stmt->line, stmt->column);
                    auto otherAccess2 = std::make_shared<MemberAccessExpr>(
                        std::make_shared<IdentifierExpr>("other", stmt->line, stmt->column),
                        fieldDecl->varName, false, false, stmt->line, stmt->column);
                    
                    auto gtCmp = std::make_shared<BinaryExpr>(otherAccess2, ltOp, selfAccess2, stmt->line, stmt->column);
                    
                    auto passFalse = std::make_shared<PassStmt>(
                        std::make_shared<LiteralExpr>(false, stmt->line, stmt->column),
                        stmt->line, stmt->column);
                    auto gtBody = std::make_shared<BlockStmt>(
                        std::vector<ASTNodePtr>{passFalse}, stmt->line, stmt->column);
                    auto gtIf = std::make_shared<IfStmt>(
                        gtCmp, gtBody, nullptr, stmt->line, stmt->column);
                    bodyStmts.push_back(gtIf);
                }
                
                // All fields equal — return false
                auto passFinal = std::make_shared<PassStmt>(
                    std::make_shared<LiteralExpr>(false, stmt->line, stmt->column),
                    stmt->line, stmt->column);
                bodyStmts.push_back(passFinal);
                
                auto body = std::make_shared<BlockStmt>(bodyStmts, stmt->line, stmt->column);
                auto retType = std::make_shared<SimpleType>("bool", stmt->line, stmt->column);
                
                auto func = std::make_shared<FuncDeclStmt>(
                    "less_than", retType,
                    std::vector<ASTNodePtr>{selfParam, otherParam},
                    body, stmt->line, stmt->column);
                
                std::vector<ASTNodePtr> methods = {func};
                auto impl = std::make_shared<ImplDeclStmt>(
                    "Ord", stmt->structName, std::move(methods),
                    stmt->line, stmt->column);
                
                syntheticNodes.push_back(impl);
                checkImplDecl(static_cast<ImplDeclStmt*>(impl.get()));
                
            } else {
                addError("Unknown derive trait '" + traitName + 
                         "'. Available: ToString, Eq, Hash, Clone, Debug, Ord", stmt);
            }
        }
    }
}

void TypeChecker::processAttributes(FuncDeclStmt* stmt) {
    if (!stmt) return;
    
    for (const auto& attr : stmt->attributes) {
        if (attr.name == "inline") {
            stmt->isInline = true;
        } else if (attr.name == "noinline") {
            stmt->isNoInline = true;
        } else if (attr.name == "gpu_kernel") {
            stmt->isGPUKernel = true;
        } else if (attr.name == "gpu_device") {
            stmt->isGPUDevice = true;
        } else if (attr.name == "comptime") {
            stmt->isComptime = true;
        } else if (attr.name == "align" || attr.name == "derive") {
            // These are handled elsewhere (struct level)
        } else {
            addError("Unknown attribute '#[" + attr.name + "]' on function", stmt);
        }
    }
}

} // namespace sema
} // namespace aria
