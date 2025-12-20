#include "frontend/sema/type.h"
#include <sstream>

namespace aria {
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
    
    // Exact match
    if (equals(target)) {
        return true;
    }
    
    // Only allow coercion between primitive types
    if (target->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    const PrimitiveType* targetPrim = static_cast<const PrimitiveType*>(target);
    
    // ========================================================================
    // TBB Type Coercion (Session 14)
    // ========================================================================
    // Allow signed integers to be assigned to TBB types of same or larger width
    // This enables: tbb8:x = 100 (where 100 is int64)
    // Runtime will validate range and convert to ERR on overflow
    
    if (targetPrim->isTBBType()) {
        // Allow signed integers to coerce to TBB
        if (isSigned && !isFloating && !isTBB) {
            // int64 -> tbb8, tbb16, tbb32, tbb64 all allowed
            // int32 -> tbb32, tbb64 allowed
            // int8 -> tbb8, tbb16, tbb32, tbb64 allowed
            // Runtime will check range and set ERR on overflow
            return bitWidth <= targetPrim->getBitWidth();
        }
        
        // Allow other TBB types to assign (for intermediate calculations)
        if (isTBB) {
            return bitWidth <= targetPrim->getBitWidth();
        }
    }
    
    // ========================================================================
    // Balanced Ternary & Nonary Type Coercion (Session 15)
    // ========================================================================
    // Allow signed integers to be assigned to ternary/nonary types
    // trit: -1, 0, 1 (balanced ternary digit)
    // tryte: 10 trits in uint16
    // nit: -4 to 4 (nonary digit)
    // nyte: 5 nits in uint16
    
    std::string targetName = targetPrim->getName();
    if (targetName == "trit" || targetName == "tryte" || 
        targetName == "nit" || targetName == "nyte") {
        // Allow signed integers to coerce to balanced types
        if (isSigned && !isFloating && !isTBB) {
            // Runtime will check range
            return true;
        }
    }
    
    // ========================================================================
    // Standard Integer Coercion
    // ========================================================================
    // Numeric widening (int8 -> int32 -> int64)
    if (!isFloating && !targetPrim->isFloatingType()) {
        // Both are integers
        if (isSigned == targetPrim->isSignedType() && !isTBB && !targetPrim->isTBBType()) {
            // Same signedness, allow widening
            return bitWidth <= targetPrim->getBitWidth();
        }
    }
    
    // ========================================================================
    // Float Coercion
    // ========================================================================
    // Float widening (flt32 -> flt64)
    if (isFloating && targetPrim->isFloatingType()) {
        return bitWidth <= targetPrim->getBitWidth();
    }
    
    // Integer to float (int32 -> flt32, int64 -> flt64)
    if (!isFloating && targetPrim->isFloatingType() && !isTBB) {
        return true;  // Allow any integer to float (may lose precision)
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
    ss << pointeeType->toString();
    ss << "@";
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
    
    // Array types must match exactly (including size)
    return equals(target);
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
    
    // Function types must match exactly for now
    // TODO Phase 3.4: Implement contravariance for parameters, covariance for return
    return equals(target);
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
    ss << "result<" << valueType->toString() << ">";
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
    
    // Signed integers: int1 through int512
    for (int bits : {1, 8, 16, 32, 64, 128, 256, 512}) {
        std::string name = "int" + std::to_string(bits);
        auto type = std::make_unique<PrimitiveType>(name, bits, true, false, false);
        primitiveCache[name] = type.get();
        types.push_back(std::move(type));
    }
    
    // Unsigned integers: uint8 through uint512
    for (int bits : {8, 16, 32, 64, 128, 256, 512}) {
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
    
    // Balanced Ternary types (trit, tryte) and Nonary types (nit, nyte)
    // trit: Single balanced ternary digit (-1, 0, 1) - 3 states in 3 bits (extra bit for overflow detection)
    auto tritType = std::make_unique<PrimitiveType>("trit", 3, true, false, false);
    primitiveCache["trit"] = tritType.get();
    types.push_back(std::move(tritType));
    
    // tryte: 10 trits packed in uint16 (16 bits for 10 ternary digits)
    auto tryteType = std::make_unique<PrimitiveType>("tryte", 16, false, false, false);
    primitiveCache["tryte"] = tryteType.get();
    types.push_back(std::move(tryteType));
    
    // nit: Single nonary digit (-4 to 4) - 9 states in 4 bits
    auto nitType = std::make_unique<PrimitiveType>("nit", 4, true, false, false);
    primitiveCache["nit"] = nitType.get();
    types.push_back(std::move(nitType));
    
    // nyte: 5 nits packed in uint16 (16 bits for 5 nonary digits)
    auto nyteType = std::make_unique<PrimitiveType>("nyte", 16, false, false, false);
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
}

PrimitiveType* TypeSystem::getPrimitiveType(const std::string& name) {
    auto it = primitiveCache.find(name);
    if (it != primitiveCache.end()) {
        return it->second;
    }
    
    // Create new primitive type if not cached
    auto type = std::make_unique<PrimitiveType>(name);
    PrimitiveType* ptr = type.get();
    primitiveCache[name] = ptr;
    types.push_back(std::move(type));
    return ptr;
}

PointerType* TypeSystem::getPointerType(Type* pointeeType, bool isMutable, bool isWild) {
    // TODO: Implement caching for pointer types
    auto type = std::make_unique<PointerType>(pointeeType, isMutable, isWild);
    PointerType* ptr = type.get();
    types.push_back(std::move(type));
    return ptr;
}

ArrayType* TypeSystem::getArrayType(Type* elementType, int size) {
    // TODO: Implement caching for array types
    auto type = std::make_unique<ArrayType>(elementType, size);
    ArrayType* ptr = type.get();
    types.push_back(std::move(type));
    return ptr;
}

VectorType* TypeSystem::getVectorType(Type* componentType, int dimension) {
    // TODO: Implement caching for vector types
    auto type = std::make_unique<VectorType>(componentType, dimension);
    VectorType* ptr = type.get();
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
    
    // TODO: Implement caching for function types
    auto type = std::make_unique<FunctionType>(paramTypes, actualReturnType, isAsync, isVariadic);
    FunctionType* ptr = type.get();
    types.push_back(std::move(type));
    return ptr;
}

OptionalType* TypeSystem::getOptionalType(Type* wrappedType) {
    // TODO: Implement caching for optional types
    // For now, create a new one each time
    auto type = std::make_unique<OptionalType>(wrappedType);
    OptionalType* ptr = type.get();
    types.push_back(std::move(type));
    return ptr;
}

ResultType* TypeSystem::getResultType(Type* valueType) {
    // TODO: Implement caching for result types
    auto type = std::make_unique<ResultType>(valueType);
    ResultType* ptr = type.get();
    types.push_back(std::move(type));
    return ptr;
}

FutureType* TypeSystem::getFutureType(Type* outputType) {
    // TODO: Implement caching for future types
    auto type = std::make_unique<FutureType>(outputType);
    FutureType* ptr = type.get();
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

} // namespace sema
} // namespace aria
