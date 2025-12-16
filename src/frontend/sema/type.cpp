#include "frontend/sema/type.h"

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
    // For now, exact match required
    // TODO: Phase 3.2 - Add type coercion rules (int8 -> int32, etc.)
    return equals(target);
}

// ============================================================================
// UnknownType Implementation
// ============================================================================

bool UnknownType::equals(const Type* other) const {
    return other && other->getKind() == TypeKind::UNKNOWN;
}

bool UnknownType::isAssignableTo(const Type* target) const {
    // Unknown type can be assigned to anything during type inference
    return true;
}

// ============================================================================
// ErrorType Implementation
// ============================================================================

bool ErrorType::equals(const Type* other) const {
    return other && other->getKind() == TypeKind::ERROR;
}

bool ErrorType::isAssignableTo(const Type* target) const {
    // Error type is compatible with everything to prevent cascading errors
    return true;
}

// ============================================================================
// TypeSystem Implementation
// ============================================================================

TypeSystem::TypeSystem() {
    // Create singleton special types
    auto unknown = std::make_unique<UnknownType>();
    unknownType = unknown.get();
    types.push_back(std::move(unknown));
    
    auto error = std::make_unique<ErrorType>();
    errorType = error.get();
    types.push_back(std::move(error));
    
    // Pre-create common primitive types
    // Reference: research_012 (Standard Integer Types)
    // Signed integers
    getPrimitiveType("int1");
    getPrimitiveType("int2");
    getPrimitiveType("int4");
    getPrimitiveType("int8");
    getPrimitiveType("int16");
    getPrimitiveType("int32");
    getPrimitiveType("int64");
    getPrimitiveType("int128");
    getPrimitiveType("int256");
    getPrimitiveType("int512");
    
    // Unsigned integers
    getPrimitiveType("uint8");
    getPrimitiveType("uint16");
    getPrimitiveType("uint32");
    getPrimitiveType("uint64");
    getPrimitiveType("uint128");
    getPrimitiveType("uint256");
    getPrimitiveType("uint512");
    
    // Floating point (research_013)
    getPrimitiveType("flt32");
    getPrimitiveType("flt64");
    getPrimitiveType("flt128");
    getPrimitiveType("flt256");
    getPrimitiveType("flt512");
    
    // TBB types (research_002)
    getPrimitiveType("tbb8");
    getPrimitiveType("tbb16");
    getPrimitiveType("tbb32");
    getPrimitiveType("tbb64");
    
    // Other primitives
    getPrimitiveType("bool");
    getPrimitiveType("string");
    getPrimitiveType("obj");
    getPrimitiveType("dyn");
}

PrimitiveType* TypeSystem::getPrimitiveType(const std::string& name) {
    // Check cache first
    auto it = primitiveCache.find(name);
    if (it != primitiveCache.end()) {
        return it->second;
    }
    
    // Create new primitive type
    auto type = std::make_unique<PrimitiveType>(name);
    PrimitiveType* typePtr = type.get();
    types.push_back(std::move(type));
    primitiveCache[name] = typePtr;
    
    return typePtr;
}

} // namespace sema
} // namespace aria
