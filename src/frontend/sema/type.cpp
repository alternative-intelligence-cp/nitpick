#include "frontend/sema/type.h"
#include <sstream>

namespace npk {
namespace sema {

// ============================================================================
// PrimitiveType Implementation
// ============================================================================

bool PrimitiveType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    const PrimitiveType* otherPrim = static_cast<const PrimitiveType*>(other);
    return name == otherPrim->name;
}

bool PrimitiveType::isAssignableTo(const Type* target) const {
    if (!target) {
        return false;
    }

    // Exact match - always allowed
    if (equals(target)) {
        return true;
    }

    // Only allow coercion between primitive types
    if (target->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }

    const PrimitiveType* targetPrim = static_cast<const PrimitiveType*>(target);

    // ========================================================================
    // Aria Type Coercion Rules (Safe Implicit Widening)
    // ========================================================================
    // Aria enforces type safety with safe implicit widening:
    // - Integer widening within same signedness family IS allowed
    //   (int8 → int16 → int32 → int64, uint8 → uint16 → uint32 → uint64)
    // - No cross-signedness coercion (int32 → uint64 NOT allowed)
    // - No narrowing (int64 → int32 requires explicit cast)
    // - No cross-family coercion (int ↔ tbb ↔ balanced NOT allowed)
    // - TBB types only match exact same type (sentinel semantics)
    // - Balanced types only coerce within Balanced sub-family
    // - Float widening IS allowed (flt32 → flt64)
    // - Literals are handled specially in TypeChecker (range-checked)
    // ========================================================================

    // ========================================================================
    // TBB Type Family - NO implicit coercion (even between TBB sizes)
    // ========================================================================
    // ARIA-018: TBB sizes have different sentinel values:
    //   tbb8 sentinel = -128, tbb16 sentinel = -32768, etc.
    // Widening -128 from tbb8 to tbb16 would create a valid value,
    // not a sentinel, breaking sticky error semantics.
    // Use explicit tbb_widen<T>() functions for size conversion.
    if (targetPrim->isTBBType()) {
        if (isTBB) {
            // REJECT: tbb8 → tbb16, etc. Only allow exact same type
            return name == targetPrim->getName();
        }
        // REJECT: int → tbb, balanced → tbb, float → tbb
        return false;
    }

    // If source is TBB, reject all other targets
    if (isTBB) {
        // REJECT: tbb → int, tbb → balanced, tbb → float
        return false;
    }

    // ========================================================================
    // Balanced Type Family (trit, tryte, nit, nyte)
    // ========================================================================
    std::string targetName = targetPrim->getName();
    bool targetIsBalanced = (targetName == "trit" || targetName == "tryte" ||
                             targetName == "nit" || targetName == "nyte");
    bool sourceIsBalanced = (name == "trit" || name == "tryte" ||
                             name == "nit" || name == "nyte");

    if (targetIsBalanced) {
        // Only allow Balanced → Balanced of same sub-family (ternary or nonary)
        if (sourceIsBalanced) {
            bool sourceIsTernary = (name == "trit" || name == "tryte");
            bool targetIsTernary = (targetName == "trit" || targetName == "tryte");
            // Only widen within same sub-family: trit → tryte, nit → nyte
            if (sourceIsTernary == targetIsTernary) {
                // trit (2 bits) → tryte (16 bits), nit (4 bits) → nyte (16 bits)
                return bitWidth <= targetPrim->getBitWidth();
            }
        }
        // REJECT: int → balanced, tbb → balanced (handled above), ternary ↔ nonary
        return false;
    }

    // If source is Balanced, reject all other targets
    if (sourceIsBalanced) {
        // REJECT: balanced → int, balanced → float
        return false;
    }

    // ========================================================================
    // Float Coercion (only widening within float family)
    // ========================================================================
    // Float widening IS allowed (flt32 → flt64)
    if (isFloating && targetPrim->isFloatingType()) {
        return bitWidth <= targetPrim->getBitWidth();
    }

    // REJECT: Integer to float (int32 → flt32 requires explicit cast)
    // REJECT: Float to integer

    // ========================================================================
    // Standard Integer Widening (safe, same signedness family only)
    // ========================================================================
    // Allow implicit widening within the same signedness family:
    //   int8 → int16 → int32 → int64   (signed)
    //   uint8 → uint16 → uint32 → uint64 (unsigned)
    // REJECT: cross-signedness (int32 → uint64), narrowing (int64 → int32)
    if (!isFloating && !targetPrim->isFloatingType()) {
        if (isSigned == targetPrim->isSignedType()) {
            return bitWidth <= targetPrim->getBitWidth();
        }
    }

    return false;
}

// ============================================================================
// PointerType Implementation
// ============================================================================

bool PointerType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::POINTER) {
        return false;
    }
    const PointerType* otherPtr = static_cast<const PointerType*>(other);
    // Erased pointers: only equal to other erased pointers (same wildness)
    if (isErased || otherPtr->isErasedPointer()) {
        return isErased == otherPtr->isErasedPointer() &&
               isMutable == otherPtr->isMutable &&
               isWild == otherPtr->isWild;
    }
    return pointeeType->equals(otherPtr->pointeeType) &&
           isMutable == otherPtr->isMutable &&
           isWild == otherPtr->isWild;
}

bool PointerType::isAssignableTo(const Type* target) const {
    if (!target) {
        return false;
    }
    
    if (target->getKind() != TypeKind::POINTER) {
        return false;
    }
    
    const PointerType* targetPtr = static_cast<const PointerType*>(target);
    
    // ARIA-P3: Any typed pointer is implicitly assignable to an erased pointer (?->/?*)
    // This mirrors C's implicit T* -> void* promotion.
    // The reverse (?-> -> T->) requires an explicit cast<T->(p).
    if (targetPtr->isErasedPointer()) {
        // Wild rule still applies: wild T@ → wild ?@, but not wild T@ → ?@
        if (isWild != targetPtr->isWild) {
            return false;
        }
        return true;
    }
    
    // Erased pointer cannot be assigned to a typed pointer without an explicit cast
    if (isErased && !targetPtr->isErasedPointer()) {
        return false;
    }
    
    // Pointee types must be compatible
    if (!pointeeType->equals(targetPtr->pointeeType)) {
        return false;
    }
    
    // Cannot assign mutable reference to immutable
    if (isMutable && !targetPtr->isMutable) {
        return false;
    }
    
    // Wild pointers have different rules
    if (isWild != targetPtr->isWild) {
        return false;
    }
    
    return true;
}

std::string PointerType::toString() const {
    std::stringstream ss;
    if (isWild) {
        ss << "wild ";
    }
    if (isErased) {
        // ARIA-P3: type-erased pointer (?-> or ?*)
        ss << "?@";
    } else {
        ss << pointeeType->toString();
        ss << "@";
    }
    if (isMutable) {
        ss << "mut";
    }
    return ss.str();
}

// ============================================================================
// ArrayType Implementation
// ============================================================================

bool ArrayType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::ARRAY) {
        return false;
    }
    const ArrayType* otherArray = static_cast<const ArrayType*>(other);
    return elementType->equals(otherArray->elementType) &&
           size == otherArray->size;
}

bool ArrayType::isAssignableTo(const Type* target) const {
    if (!target) {
        return false;
    }
    
    if (target->getKind() != TypeKind::ARRAY) {
        return false;
    }
    
    const ArrayType* targetArray = static_cast<const ArrayType*>(target);
    
    // Element types must match
    if (!elementType->equals(targetArray->elementType)) {
        return false;
    }
    
    // Allow fixed-size to dynamic: int32[3] → int32[]
    if (targetArray->isDynamic()) {
        return true;
    }
    
    // Otherwise require exact size match
    return size == targetArray->size;
}

std::string ArrayType::toString() const {
    std::stringstream ss;
    ss << elementType->toString();
    ss << "[";
    if (size >= 0) {
        ss << size;
    }
    ss << "]";
    return ss.str();
}

// ============================================================================
// VectorType Implementation
// ============================================================================

bool VectorType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::VECTOR) {
        return false;
    }
    const VectorType* otherVec = static_cast<const VectorType*>(other);
    return componentType->equals(otherVec->componentType) &&
           dimension == otherVec->dimension;
}

bool VectorType::isAssignableTo(const Type* target) const {
    if (!target) {
        return false;
    }
    
    // Vector types must match exactly
    return equals(target);
}

std::string VectorType::toString() const {
    std::stringstream ss;
    
    // Determine prefix based on component type
    std::string compName = componentType->toString();
    if (compName == "flt32") {
        ss << "vec" << dimension;
    } else if (compName == "flt64") {
        ss << "dvec" << dimension;
    } else if (compName == "int32") {
        ss << "ivec" << dimension;
    } else if (compName == "bool") {
        ss << "bvec" << dimension;
    } else {
        ss << compName << "vec" << dimension;
    }
    
    return ss.str();
}

// ============================================================================
// FunctionType Implementation
// ============================================================================

bool FunctionType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::FUNCTION) {
        return false;
    }
    const FunctionType* otherFunc = static_cast<const FunctionType*>(other);
    
    // Check parameter count
    if (paramTypes.size() != otherFunc->paramTypes.size()) {
        return false;
    }
    
    // Check all parameter types
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (!paramTypes[i]->equals(otherFunc->paramTypes[i])) {
            return false;
        }
    }
    
    // Check return type
    if (!returnType->equals(otherFunc->returnType)) {
        return false;
    }
    
    // Check async and variadic flags
    if (isAsync != otherFunc->isAsync || isVariadic != otherFunc->isVariadic) {
        return false;
    }
    
    return true;
}

bool FunctionType::isAssignableTo(const Type* target) const {
    if (!target) {
        return false;
    }
    
    // Function-to-pointer coercion: function refs can be passed as erased pointers
    // for FFI callbacks (e.g., thread spawn). Codegen extracts method_ptr from fat pointer.
    if (target->getKind() == TypeKind::POINTER) {
        const PointerType* ptrTarget = static_cast<const PointerType*>(target);
        if (ptrTarget->isErasedPointer()) {
            return true;
        }
    }
    
    // Function types must match exactly, or satisfy variance rules:
    // - Parameters are contravariant (target params assignable to source params)
    // - Return type is covariant (source return assignable to target return)
    if (target->getKind() != TypeKind::FUNCTION) {
        return false;
    }
    
    const FunctionType* targetFunc = static_cast<const FunctionType*>(target);
    
    // Exact match is always fine
    if (equals(target)) {
        return true;
    }
    
    // Async and variadic flags must match
    if (isAsync != targetFunc->isAsync || isVariadic != targetFunc->isVariadic) {
        return false;
    }
    
    // Parameter count must match
    if (paramTypes.size() != targetFunc->paramTypes.size()) {
        return false;
    }
    
    // Parameters are contravariant: target's param types must be assignable
    // to source's param types (reversed direction)
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (!targetFunc->paramTypes[i]->isAssignableTo(paramTypes[i])) {
            return false;
        }
    }
    
    // Return type is covariant: source's return type must be assignable
    // to target's return type
    if (!returnType->isAssignableTo(targetFunc->returnType)) {
        return false;
    }
    
    return true;
}

std::string FunctionType::toString() const {
    std::stringstream ss;
    
    if (isAsync) {
        ss << "async ";
    }
    
    ss << "func(";
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (i > 0) ss << ", ";
        ss << paramTypes[i]->toString();
    }
    if (isVariadic) {
        if (!paramTypes.empty()) ss << ", ";
        ss << "...";
    }
    ss << ") -> " << returnType->toString();
    
    return ss.str();
}

// ============================================================================
// StructType Implementation
// ============================================================================

const StructType::Field* StructType::getField(const std::string& fieldName) const {
    for (const auto& field : fields) {
        if (field.name == fieldName) {
            return &field;
        }
    }
    return nullptr;
}

int StructType::getFieldIndex(const std::string& fieldName) const {
    for (size_t i = 0; i < fields.size(); i++) {
        if (fields[i].name == fieldName) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool StructType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::STRUCT) {
        return false;
    }
    const StructType* otherStruct = static_cast<const StructType*>(other);
    
    // Struct types are nominal - compare by name
    return name == otherStruct->name;
}

bool StructType::isAssignableTo(const Type* target) const {
    if (!target) {
        return false;
    }
    
    // Struct types must match exactly (nominal typing)
    return equals(target);
}

std::string StructType::toString() const {
    // Just return the struct name (no "struct " prefix)
    // This allows types to match: Point == Point, not Point != struct Point
    return name;
}

// ============================================================================
// UnionType Implementation
// ============================================================================

bool UnionType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::UNION) {
        return false;
    }
    const UnionType* otherUnion = static_cast<const UnionType*>(other);
    
    // Union types are nominal - compare by name
    return name == otherUnion->name;
}

bool UnionType::isAssignableTo(const Type* target) const {
    if (!target) {
        return false;
    }
    
    // Union types must match exactly (nominal typing)
    return equals(target);
}

std::string UnionType::toString() const {
    std::stringstream ss;
    ss << "union " << name;
    return ss.str();
}

// ============================================================================
// GenericType Implementation
// ============================================================================

bool GenericType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::GENERIC) {
        return false;
    }
    const GenericType* otherGeneric = static_cast<const GenericType*>(other);
    return name == otherGeneric->name;
}

bool GenericType::isAssignableTo(const Type* /*target*/) const {
    // Generic types can be assigned to anything (will be resolved during monomorphization)
    return true;
}

// ============================================================================
// Dimension Implementation (P1-5)
// ============================================================================

std::string Dimension::toString() const {
    if (isDimensionless()) {
        return "Dimensionless";
    }
    
    std::stringstream ss;
    bool first = true;
    
    // Build dimension string from exponents
    auto addDimension = [&](const std::string& name, int8_t exponent) {
        if (exponent == 0) return;
        
        if (!first) ss << "⋅";
        first = false;
        
        ss << name;
        if (exponent != 1) {
            // Use superscript notation for exponents
            ss << "^" << static_cast<int>(exponent);
        }
    };
    
    // Add each non-zero dimension
    addDimension("m", length);
    addDimension("kg", mass);
    addDimension("s", time);
    addDimension("A", current);
    addDimension("K", temperature);
    addDimension("mol", amount);
    addDimension("cd", luminosity);
    
    return ss.str();
}

// ============================================================================
// DimensionalType Implementation (P1-5)
// ============================================================================

bool DimensionalType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::DIMENSIONAL) {
        return false;
    }
    const DimensionalType* otherDim = static_cast<const DimensionalType*>(other);
    
    // Must have same base type and same dimension
    return baseType->equals(otherDim->baseType) && 
           dimension == otherDim->dimension;
}

bool DimensionalType::isAssignableTo(const Type* target) const {
    if (!target) {
        return false;
    }
    
    // Can assign to identical dimensional type
    if (target->getKind() == TypeKind::DIMENSIONAL) {
        return equals(target);
    }
    
    // Can assign dimensionless to base type
    if (isDimensionless() && baseType->equals(target)) {
        return true;
    }
    
    // Cannot assign dimensional types to non-dimensional types
    return false;
}

std::string DimensionalType::toString() const {
    std::stringstream ss;
    ss << baseType->toString() << "<";
    
    if (!dimensionName.empty()) {
        ss << dimensionName;
    } else {
        ss << dimension.toString();
    }
    
    ss << ">";
    return ss.str();
}

// ============================================================================
// OptionalType Implementation
// ============================================================================

bool OptionalType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::OPTIONAL) {
        return false;
    }
    const OptionalType* otherOptional = static_cast<const OptionalType*>(other);
    return wrappedType->equals(otherOptional->wrappedType);
}

bool OptionalType::isAssignableTo(const Type* target) const {
    if (!target) {
        return false;
    }
    
    // Can assign T? to T? if T matches
    if (target->getKind() == TypeKind::OPTIONAL) {
        const OptionalType* targetOptional = static_cast<const OptionalType*>(target);
        return wrappedType->isAssignableTo(targetOptional->wrappedType);
    }
    
    // Cannot assign T? to T (requires explicit unwrapping)
    return false;
}

std::string OptionalType::toString() const {
    return wrappedType->toString() + "?";
}

// ============================================================================
// ResultType Implementation
// ============================================================================

bool ResultType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::RESULT) {
        return false;
    }
    const ResultType* otherResult = static_cast<const ResultType*>(other);
    return valueType->equals(otherResult->valueType);
}

bool ResultType::isAssignableTo(const Type* target) const {
    if (!target) {
        return false;
    }
    
    // Result types must match exactly
    return equals(target);
}

std::string ResultType::toString() const {
    std::stringstream ss;
    ss << "Result<" << valueType->toString() << ">";
    return ss.str();
}

// ============================================================================
// FutureType Implementation
// ============================================================================

bool FutureType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::FUTURE) {
        return false;
    }
    const FutureType* otherFuture = static_cast<const FutureType*>(other);
    return outputType->equals(otherFuture->outputType);
}

bool FutureType::isAssignableTo(const Type* target) const {
    if (!target) {
        return false;
    }
    
    // Future types must match exactly
    return equals(target);
}

std::string FutureType::toString() const {
    std::stringstream ss;
    ss << "future<" << outputType->toString() << ">";
    return ss.str();
}

// ============================================================================
// HandleType Implementation (P1-3: Generational Handles)
// ============================================================================

bool HandleType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::HANDLE) {
        return false;
    }
    const HandleType* otherHandle = static_cast<const HandleType*>(other);
    return pointeeType->equals(otherHandle->pointeeType);
}

bool HandleType::isAssignableTo(const Type* target) const {
    if (!target) {
        return false;
    }
    
    // Handle<T> can assign to Handle<T> if T matches
    if (target->getKind() == TypeKind::HANDLE) {
        const HandleType* targetHandle = static_cast<const HandleType*>(target);
        return pointeeType->isAssignableTo(targetHandle->pointeeType);
    }
    
    // Cannot assign Handle<T> to T (requires explicit .get())
    return false;
}

std::string HandleType::toString() const {
    std::stringstream ss;
    ss << "Handle<" << pointeeType->toString() << ">";
    return ss.str();
}

// ============================================================================
// SimdType implementation
// ============================================================================

bool SimdType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::SIMD) {
        return false;
    }
    const SimdType* otherSimd = static_cast<const SimdType*>(other);
    // Both element type AND lane count must match
    return elementType->equals(otherSimd->elementType) && 
           laneCount == otherSimd->laneCount;
}

bool SimdType::isAssignableTo(const Type* target) const {
    if (!target) {
        return false;
    }
    
    // simd<T, N> can only assign to simd<T, N> with exact match
    if (target->getKind() == TypeKind::SIMD) {
        return equals(target);  // Must be exact match
    }
    
    // Cannot assign simd<T, N> to T or T[] (requires explicit conversion)
    return false;
}

std::string SimdType::toString() const {
    std::stringstream ss;
    ss << "simd<" << elementType->toString() << ", " << laneCount << ">";
    return ss.str();
}

// ============================================================================
// AnyType Implementation
// ============================================================================

bool AnyType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::ANY) return false;
    auto* otherAny = static_cast<const AnyType*>(other);
    return isWild == otherAny->isWild && isWildx == otherAny->isWildx;
}

bool AnyType::isAssignableTo(const Type* target) const {
    if (!target) return false;
    // any can be assigned to any
    if (target->getKind() == TypeKind::ANY) return true;
    // any cannot be implicitly assigned to concrete types — must use resolve()
    return false;
}

std::string AnyType::toString() const {
    if (isWildx) return "wildx any";
    if (isWild) return "wild any";
    return "any";
}

// ============================================================================
// DynTraitType Implementation
// ============================================================================

bool DynTraitType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::DYN_TRAIT) return false;
    auto* otherDyn = static_cast<const DynTraitType*>(other);
    return traitName == otherDyn->traitName;
}

bool DynTraitType::isAssignableTo(const Type* target) const {
    if (!target) return false;
    // dyn Trait can be assigned to same dyn Trait
    if (target->getKind() == TypeKind::DYN_TRAIT) {
        auto* targetDyn = static_cast<const DynTraitType*>(target);
        return traitName == targetDyn->traitName;
    }
    return false;
}

std::string DynTraitType::toString() const {
    return "dyn " + traitName;
}

// ============================================================================
// EnumType Implementation (v0.2.39)
// ============================================================================

bool EnumType::equals(const Type* other) const {
    if (!other || other->getKind() != TypeKind::ENUM) return false;
    auto* otherEnum = static_cast<const EnumType*>(other);
    return name == otherEnum->name;
}

bool EnumType::isAssignableTo(const Type* target) const {
    if (!target) return false;
    // Enum can be assigned to same enum type
    if (target->getKind() == TypeKind::ENUM) {
        auto* targetEnum = static_cast<const EnumType*>(target);
        return name == targetEnum->name;
    }
    // Enum can be assigned to int64 (implicit widening for interop)
    if (target->getKind() == TypeKind::PRIMITIVE) {
        auto* prim = static_cast<const PrimitiveType*>(target);
        return prim->getName() == "int64";
    }
    return false;
}

std::string EnumType::toString() const {
    return name;
}

// ============================================================================
// UnknownType Implementation
// ============================================================================

bool UnknownType::equals(const Type* other) const {
    return other && other->getKind() == TypeKind::UNKNOWN;
}

bool UnknownType::isAssignableTo(const Type* /*target*/) const {
    // Unknown types can be assigned to anything (type inference will resolve)
    return true;
}

// ============================================================================
// ErrorType Implementation
// ============================================================================

bool ErrorType::equals(const Type* other) const {
    return other && other->getKind() == TypeKind::ERROR;
}

bool ErrorType::isAssignableTo(const Type* /*target*/) const {
    // Error types can be assigned to anything (prevent cascading errors)
    return true;
}

// ============================================================================
// TypeSystem Implementation
// ============================================================================

TypeSystem::TypeSystem() {
    // Create special types
    auto unknown = std::make_unique<UnknownType>();
    unknownType = unknown.get();
    types.push_back(std::move(unknown));
    
    auto error = std::make_unique<ErrorType>();
    errorType = error.get();
    types.push_back(std::move(error));
    
    // Pre-create common primitive types
    // Reference: research_012 (integers), research_013 (floats), research_002 (TBB)
    
    // Signed integers: int1 through int512 (full power-of-two coverage)
    for (int bits : {1, 2, 4, 8, 16, 32, 64, 128, 256, 512}) {
        std::string name = "int" + std::to_string(bits);
        auto type = std::make_unique<PrimitiveType>(name, bits, true, false, false);
        primitiveCache[name] = type.get();
        types.push_back(std::move(type));
    }
    
    // Unsigned integers: uint1 through uint512 (full power-of-two coverage)
    for (int bits : {1, 2, 4, 8, 16, 32, 64, 128, 256, 512}) {
        std::string name = "uint" + std::to_string(bits);
        auto type = std::make_unique<PrimitiveType>(name, bits, false, false, false);
        primitiveCache[name] = type.get();
        types.push_back(std::move(type));
    }
    
    // Floating point: flt32 through flt512
    for (int bits : {32, 64, 128, 256, 512}) {
        std::string name = "flt" + std::to_string(bits);
        auto type = std::make_unique<PrimitiveType>(name, bits, true, true, false);
        primitiveCache[name] = type.get();
        types.push_back(std::move(type));
    }
    
    // TBB (Twisted Balanced Binary) types: tbb8, tbb16, tbb32, tbb64
    for (int bits : {8, 16, 32, 64}) {
        std::string name = "tbb" + std::to_string(bits);
        auto type = std::make_unique<PrimitiveType>(name, bits, true, false, true);
        primitiveCache[name] = type.get();
        types.push_back(std::move(type));
    }
    
    // Frac (Exact Rational) types: frac8, frac16, frac32, frac64
    // Each stores {tbbN whole, tbbN numerator, tbbN denominator}
    for (int bits : {8, 16, 32, 64}) {
        std::string name = "frac" + std::to_string(bits);
        // Size is 3x the underlying TBB size (whole + num + denom)
        auto type = std::make_unique<PrimitiveType>(name, bits * 3, true, false, false);
        primitiveCache[name] = type.get();
        types.push_back(std::move(type));
    }
    
    // TFP (Twisted Floating Point) types: tfp32, tfp64
    // tfp32: {tbb16 exponent, tbb16 mantissa} = 32 bits
    auto tfp32Type = std::make_unique<PrimitiveType>("tfp32", 32, true, true, false);
    primitiveCache["tfp32"] = tfp32Type.get();
    types.push_back(std::move(tfp32Type));
    
    // tfp64: {tbb16 exponent, tbb48 mantissa, 0 padding} = 64 bits
    auto tfp64Type = std::make_unique<PrimitiveType>("tfp64", 64, true, true, false);
    primitiveCache["tfp64"] = tfp64Type.get();
    types.push_back(std::move(tfp64Type));
    
    // Fixed-point types (Q128.128 deterministic arithmetic for physics)
    // fix256: 256-bit fixed-point (128 integer bits + 128 fractional bits)
    auto fix256Type = std::make_unique<PrimitiveType>("fix256", 256, true, false, false);
    primitiveCache["fix256"] = fix256Type.get();
    types.push_back(std::move(fix256Type));
    
    // Vec9: 16x tbb32 elements (512 bits, 64 bytes)
    auto vec9Type = std::make_unique<PrimitiveType>("vec9", 512, false, false, false);
    primitiveCache["vec9"] = vec9Type.get();
    types.push_back(std::move(vec9Type));
    
    // TMatrix: Sentinel-aware matrix (composite structure)
    auto tmatrixType = std::make_unique<PrimitiveType>("tmatrix", 0, false, false, false);
    primitiveCache["tmatrix"] = tmatrixType.get();
    types.push_back(std::move(tmatrixType));
    
    // Initialize dimension registry (P1-5)
    initializeDimensionRegistry();
    
    // TTensor: 9D toroidal tensor (composite structure)
    auto ttensorType = std::make_unique<PrimitiveType>("ttensor", 0, false, false, false);
    primitiveCache["ttensor"] = ttensorType.get();
    types.push_back(std::move(ttensorType));
    
    // Exotic Balanced Base types (balanced ternary and nonary)
    // Reference: docs/gemini/responses/remaining/Exotic Balanced Base Arithmetic Implementation.txt
    
    // trit: Single balanced ternary digit (-1, 0, 1) stored as tbb8 with ERR=-128
    auto tritType = std::make_unique<PrimitiveType>("trit", 8, true, false, false, true);
    primitiveCache["trit"] = tritType.get();
    types.push_back(std::move(tritType));
    
    // tryte: 10 trits (59,049 values) stored in uint16 with biased representation
    // Bias: 29,524, Range: ±29,524, Sentinel: 0xFFFF
    auto tryteType = std::make_unique<PrimitiveType>("tryte", 16, false, false, false, true);
    primitiveCache["tryte"] = tryteType.get();
    types.push_back(std::move(tryteType));
    
    // nit: Single nonary digit (base 9: -4 to +4) stored as int8
    auto nitType = std::make_unique<PrimitiveType>("nit", 8, true, false, false, true);
    primitiveCache["nit"] = nitType.get();
    types.push_back(std::move(nitType));
    
    // nyte: 5 nits (isomorphic to tryte) stored in uint16 with biased representation
    // Same bias and sentinel as tryte (59,049 values = 9^5 = 3^10)
    auto nyteType = std::make_unique<PrimitiveType>("nyte", 16, false, false, false, true);
    primitiveCache["nyte"] = nyteType.get();
    types.push_back(std::move(nyteType));
    
    // Other primitives
    auto boolType = std::make_unique<PrimitiveType>("bool", 1, false, false, false);
    primitiveCache["bool"] = boolType.get();
    types.push_back(std::move(boolType));
    
    auto stringType = std::make_unique<PrimitiveType>("string", 0, false, false, false);
    primitiveCache["string"] = stringType.get();
    types.push_back(std::move(stringType));
    
    auto objType = std::make_unique<PrimitiveType>("obj", 0, false, false, false);
    primitiveCache["obj"] = objType.get();
    types.push_back(std::move(objType));
    
    auto dynType = std::make_unique<PrimitiveType>("dyn", 0, false, false, false);
    primitiveCache["dyn"] = dynType.get();
    types.push_back(std::move(dynType));
    
    // I/O and System types (Phase 3.4)
    auto binaryType = std::make_unique<PrimitiveType>("binary", 0, false, false, false);
    primitiveCache["binary"] = binaryType.get();
    types.push_back(std::move(binaryType));
    
    auto bufferType = std::make_unique<PrimitiveType>("buffer", 0, false, false, false);
    primitiveCache["buffer"] = bufferType.get();
    types.push_back(std::move(bufferType));
    
    auto streamType = std::make_unique<PrimitiveType>("stream", 0, false, false, false);
    primitiveCache["stream"] = streamType.get();
    types.push_back(std::move(streamType));
    
    auto processType = std::make_unique<PrimitiveType>("process", 0, false, false, false);
    primitiveCache["process"] = processType.get();
    types.push_back(std::move(processType));
    
    auto pipeType = std::make_unique<PrimitiveType>("pipe", 0, false, false, false);
    primitiveCache["pipe"] = pipeType.get();
    types.push_back(std::move(pipeType));
    
    // Mathematical types (Phase 3.3) - infrastructure only
    // Full matrix/tensor implementation deferred
    auto matrixType = std::make_unique<PrimitiveType>("matrix", 0, false, false, false);
    primitiveCache["matrix"] = matrixType.get();
    types.push_back(std::move(matrixType));
    
    auto tensorType = std::make_unique<PrimitiveType>("tensor", 0, false, false, false);
    primitiveCache["tensor"] = tensorType.get();
    types.push_back(std::move(tensorType));
}

PrimitiveType* TypeSystem::getPrimitiveType(const std::string& name) {
    // Normalize type aliases to canonical names
    // This ensures that "float32" and "flt32" create the same type object
    std::string canonicalName = name;
    
    // Float aliases: float32 → flt32, float64 → flt64, etc.
    if (name == "float32") canonicalName = "flt32";
    else if (name == "float64") canonicalName = "flt64";
    else if (name == "float128") canonicalName = "flt128";
    else if (name == "float256") canonicalName = "flt256";
    else if (name == "float512") canonicalName = "flt512";
    
    auto it = primitiveCache.find(canonicalName);
    if (it != primitiveCache.end()) {
        return it->second;
    }
    
    // Create new primitive type if not cached (using canonical name)
    auto type = std::make_unique<PrimitiveType>(canonicalName);
    PrimitiveType* ptr = type.get();
    primitiveCache[canonicalName] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

PointerType* TypeSystem::getPointerType(Type* pointeeType, bool isMutable, bool isWild) {
    std::string key = (pointeeType ? pointeeType->toString() : "void") +
                      (isMutable ? "_mut" : "") + (isWild ? "_wild" : "");
    auto it = pointerCache.find(key);
    if (it != pointerCache.end()) return it->second;
    auto type = std::make_unique<PointerType>(pointeeType, isMutable, isWild, /*isErased=*/false);
    PointerType* ptr = type.get();
    pointerCache[key] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

// ARIA-P3: Type-erased pointer factory (?-> / ?*)
// Creates a sema PointerType with isErased=true and no pointee type.
PointerType* TypeSystem::getErasedPointerType(bool isMutable, bool isWild) {
    auto type = std::make_unique<PointerType>(/*pointeeType=*/nullptr, isMutable, isWild, /*isErased=*/true);
    PointerType* ptr = type.get();
    types.push_back(std::move(type));
    return ptr;
}

ArrayType* TypeSystem::getArrayType(Type* elementType, int size) {
    std::string key = (elementType ? elementType->toString() : "void") + "[" + std::to_string(size) + "]";
    auto it = arrayCache.find(key);
    if (it != arrayCache.end()) return it->second;
    auto type = std::make_unique<ArrayType>(elementType, size);
    ArrayType* ptr = type.get();
    arrayCache[key] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

VectorType* TypeSystem::getVectorType(Type* componentType, int dimension) {
    std::string key = (componentType ? componentType->toString() : "void") + "_v" + std::to_string(dimension);
    auto it = vectorCache.find(key);
    if (it != vectorCache.end()) return it->second;
    auto type = std::make_unique<VectorType>(componentType, dimension);
    VectorType* ptr = type.get();
    vectorCache[key] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

FunctionType* TypeSystem::getFunctionType(const std::vector<Type*>& paramTypes, Type* returnType,
                                         bool isAsync, bool isVariadic) {
    // Phase 4.5.3: Async functions automatically return future<T> instead of T
    // When user declares: async func:foo = int32() { ... }
    // Actual type becomes: func() -> future<int32>
    Type* actualReturnType = returnType;
    if (isAsync && returnType) {
        actualReturnType = getFutureType(returnType);
    }
    
    std::string key;
    for (auto* pt : paramTypes) key += (pt ? pt->toString() : "void") + ",";
    key += "->" + (actualReturnType ? actualReturnType->toString() : "void");
    if (isAsync) key += "_async";
    if (isVariadic) key += "_va";
    auto it = functionCache.find(key);
    if (it != functionCache.end()) return it->second;
    auto type = std::make_unique<FunctionType>(paramTypes, actualReturnType, isAsync, isVariadic);
    FunctionType* ptr = type.get();
    functionCache[key] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

OptionalType* TypeSystem::getOptionalType(Type* wrappedType) {
    std::string key = wrappedType ? wrappedType->toString() : "void";
    auto it = optionalCache.find(key);
    if (it != optionalCache.end()) return it->second;
    auto type = std::make_unique<OptionalType>(wrappedType);
    OptionalType* ptr = type.get();
    optionalCache[key] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

ResultType* TypeSystem::getResultType(Type* valueType) {
    std::string key = valueType ? valueType->toString() : "void";
    auto it = resultCache.find(key);
    if (it != resultCache.end()) return it->second;
    auto type = std::make_unique<ResultType>(valueType);
    ResultType* ptr = type.get();
    resultCache[key] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

FutureType* TypeSystem::getFutureType(Type* outputType) {
    std::string key = outputType ? outputType->toString() : "void";
    auto it = futureCache.find(key);
    if (it != futureCache.end()) return it->second;
    auto type = std::make_unique<FutureType>(outputType);
    FutureType* ptr = type.get();
    futureCache[key] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

HandleType* TypeSystem::getHandleType(Type* pointeeType) {
    std::string key = pointeeType ? pointeeType->toString() : "void";
    auto it = handleCache.find(key);
    if (it != handleCache.end()) return it->second;
    auto type = std::make_unique<HandleType>(pointeeType);
    HandleType* ptr = type.get();
    handleCache[key] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

SimdType* TypeSystem::getSimdType(Type* elementType, size_t laneCount) {
    std::string key = (elementType ? elementType->toString() : "void") + "x" + std::to_string(laneCount);
    auto it = simdCache.find(key);
    if (it != simdCache.end()) return it->second;
    auto type = std::make_unique<SimdType>(elementType, laneCount);
    SimdType* ptr = type.get();
    simdCache[key] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

AnyType* TypeSystem::getAnyType(bool isWild, bool isWildx) {
    auto type = std::make_unique<AnyType>(isWild, isWildx);
    AnyType* ptr = type.get();
    types.push_back(std::move(type));
    return ptr;
}

DynTraitType* TypeSystem::getDynTraitType(const std::string& traitName) {
    auto type = std::make_unique<DynTraitType>(traitName);
    DynTraitType* ptr = type.get();
    types.push_back(std::move(type));
    return ptr;
}

// Phase 4.5.3 Task #4: Extract T from future<T> for await expressions
// When we await a future<T>, the result type is T (not future<T>)
Type* TypeSystem::unwrapFutureType(Type* futureType) {
    if (!futureType) {
        return getErrorType();
    }
    
    // If it's a Future type, extract the output type
    if (futureType->getKind() == TypeKind::FUTURE) {
        FutureType* future = static_cast<FutureType*>(futureType);
        return future->getOutputType();
    }
    
    // If it's not a Future, this is a type error
    // await can only be used on Future types
    return getErrorType();
}

StructType* TypeSystem::getStructType(const std::string& name) {
    auto it = structCache.find(name);
    if (it != structCache.end()) {
        return it->second;
    }
    return nullptr;
}

StructType* TypeSystem::createStructType(const std::string& name, const std::vector<StructType::Field>& fields,
                                        int size, int alignment, bool isPacked) {
    auto type = std::make_unique<StructType>(name, fields, size, alignment, isPacked);
    StructType* ptr = type.get();
    structCache[name] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

void TypeSystem::registerStructAlias(const std::string& alias, StructType* type) {
    if (type && structCache.find(alias) == structCache.end()) {
        structCache[alias] = type;
    }
}

UnionType* TypeSystem::getUnionType(const std::string& name) {
    auto it = unionCache.find(name);
    if (it != unionCache.end()) {
        return it->second;
    }
    return nullptr;
}

UnionType* TypeSystem::createUnionType(const std::string& name, const std::vector<UnionType::Variant>& variants,
                                      int size) {
    auto type = std::make_unique<UnionType>(name, variants, size);
    UnionType* ptr = type.get();
    unionCache[name] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

// v0.2.39: Enum type factory methods
EnumType* TypeSystem::getEnumType(const std::string& name) {
    auto it = enumCache.find(name);
    if (it != enumCache.end()) {
        return it->second;
    }
    return nullptr;
}

EnumType* TypeSystem::createEnumType(const std::string& name, const std::map<std::string, int64_t>& variants) {
    auto type = std::make_unique<EnumType>(name, variants);
    EnumType* ptr = type.get();
    enumCache[name] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

GenericType* TypeSystem::getGenericType(const std::string& name) {
    auto it = genericCache.find(name);
    if (it != genericCache.end()) {
        return it->second;
    }
    
    // Create new generic type
    auto type = std::make_unique<GenericType>(name);
    GenericType* ptr = type.get();
    genericCache[name] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

// ============================================================================
// Dimensional Type System (P1-5)
// ============================================================================

void TypeSystem::initializeDimensionRegistry() {
    // Base SI dimensions
    dimensionRegistry["Dimensionless"] = Dimension::Dimensionless();
    dimensionRegistry["Length"] = Dimension::Length();
    dimensionRegistry["Mass"] = Dimension::Mass();
    dimensionRegistry["Time"] = Dimension::Time();
    dimensionRegistry["Current"] = Dimension::Current();
    dimensionRegistry["Temperature"] = Dimension::Temperature();
    dimensionRegistry["Amount"] = Dimension::Amount();
    dimensionRegistry["Luminosity"] = Dimension::Luminosity();
    
    // Base SI unit names
    dimensionRegistry["Meters"] = Dimension::Length();
    dimensionRegistry["Kilograms"] = Dimension::Mass();
    dimensionRegistry["Seconds"] = Dimension::Time();
    dimensionRegistry["Amperes"] = Dimension::Current();
    dimensionRegistry["Kelvin"] = Dimension::Temperature();
    dimensionRegistry["Moles"] = Dimension::Amount();
    dimensionRegistry["Candela"] = Dimension::Luminosity();
    
    // Common derived dimensions
    dimensionRegistry["Velocity"] = Dimension::Velocity();
    dimensionRegistry["Acceleration"] = Dimension::Acceleration();
    dimensionRegistry["Force"] = Dimension::Force();
    dimensionRegistry["Newtons"] = Dimension::Force();
    dimensionRegistry["Energy"] = Dimension::Energy();
    dimensionRegistry["Joules"] = Dimension::Energy();
    dimensionRegistry["Power"] = Dimension::Power();
    dimensionRegistry["Watts"] = Dimension::Power();
    dimensionRegistry["Charge"] = Dimension::Charge();
    dimensionRegistry["Coulombs"] = Dimension::Charge();
    dimensionRegistry["Voltage"] = Dimension::Voltage();
    dimensionRegistry["Volts"] = Dimension::Voltage();
    dimensionRegistry["Resistance"] = Dimension::Resistance();
    dimensionRegistry["Ohms"] = Dimension::Resistance();
    dimensionRegistry["Frequency"] = Dimension::Frequency();
    dimensionRegistry["Hertz"] = Dimension::Frequency();
    dimensionRegistry["Resonance"] = Dimension::Frequency();  // Alias for Nikola
    dimensionRegistry["Action"] = Dimension::Action();
    dimensionRegistry["Coherence"] = Dimension::Action();     // Alias for Nikola
}

const Dimension* TypeSystem::lookupDimension(const std::string& name) const {
    auto it = dimensionRegistry.find(name);
    if (it != dimensionRegistry.end()) {
        return &it->second;
    }
    return nullptr;
}

void TypeSystem::registerDimension(const std::string& name, const Dimension& dimension) {
    dimensionRegistry[name] = dimension;
}

DimensionalType* TypeSystem::getDimensionalType(Type* baseType, const Dimension& dimension, 
                                               const std::string& dimensionName) {
    // Create cache key: baseType + dimension vector
    std::string cacheKey = baseType->toString() + "<";
    if (!dimensionName.empty()) {
        cacheKey += dimensionName;
    } else {
        // Use dimension vector as key if no name
        cacheKey += dimension.toString();
    }
    cacheKey += ">";
    
    auto it = dimensionalCache.find(cacheKey);
    if (it != dimensionalCache.end()) {
        return it->second;
    }
    
    // Create new dimensional type
    auto type = std::make_unique<DimensionalType>(baseType, dimension, dimensionName);
    DimensionalType* ptr = type.get();
    dimensionalCache[cacheKey] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

DimensionalType* TypeSystem::getDimensionalType(Type* baseType, const std::string& dimensionName) {
    // Lookup dimension by name
    const Dimension* dim = lookupDimension(dimensionName);
    if (!dim) {
        // Unknown dimension name - could be an error, but for now create dimensionless
        return getDimensionalType(baseType, Dimension::Dimensionless(), dimensionName);
    }
    
    return getDimensionalType(baseType, *dim, dimensionName);
}

} // namespace sema
} // namespace npk
