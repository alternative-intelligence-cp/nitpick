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
    
    // TODO Phase 3.2.2: Implement type coercion rules
    // - Numeric widening (int8 -> int32, int32 -> int64)
    // - Float coercion (flt32 -> flt64)
    // - Integer to float (int32 -> flt32)
    // For now, require exact match
    
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
    std::stringstream ss;
    ss << "struct " << name;
    return ss.str();
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
    // TODO: Implement caching for function types
    auto type = std::make_unique<FunctionType>(paramTypes, returnType, isAsync, isVariadic);
    FunctionType* ptr = type.get();
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
