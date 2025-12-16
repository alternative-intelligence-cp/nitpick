#ifndef ARIA_SEMA_TYPE_H
#define ARIA_SEMA_TYPE_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace aria {
namespace sema {

// ============================================================================
// Type - Base class for all types in the semantic analyzer
// ============================================================================
// This is a simplified version for Phase 3.1 (Symbol Table)
// Full type system will be implemented in Phase 3.2

enum class TypeKind {
    PRIMITIVE,      // int8, int32, bool, string, etc.
    POINTER,        // T@
    ARRAY,          // T[], T[N]
    FUNCTION,       // func(params) -> return
    STRUCT,         // struct { fields }
    GENERIC,        // T, U, V (type parameters)
    UNKNOWN,        // Type not yet inferred
    ERROR,          // Type error occurred
};

class Type {
protected:
    TypeKind kind;
    
public:
    explicit Type(TypeKind kind) : kind(kind) {}
    virtual ~Type() = default;
    
    TypeKind getKind() const { return kind; }
    
    // Type operations (to be fully implemented in Phase 3.2)
    virtual bool equals(const Type* other) const = 0;
    virtual bool isAssignableTo(const Type* target) const = 0;
    virtual std::string toString() const = 0;
};

// ============================================================================
// PrimitiveType - Built-in primitive types
// ============================================================================

class PrimitiveType : public Type {
private:
    std::string name;  // "int8", "int32", "bool", "string", etc.
    
public:
    explicit PrimitiveType(const std::string& name)
        : Type(TypeKind::PRIMITIVE), name(name) {}
    
    const std::string& getName() const { return name; }
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override { return name; }
};

// ============================================================================
// UnknownType - Used during type inference
// ============================================================================

class UnknownType : public Type {
public:
    UnknownType() : Type(TypeKind::UNKNOWN) {}
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override { return "<unknown>"; }
};

// ============================================================================
// ErrorType - Represents a type error
// ============================================================================

class ErrorType : public Type {
public:
    ErrorType() : Type(TypeKind::ERROR) {}
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override { return "<error>"; }
};

// ============================================================================
// TypeSystem - Factory and cache for types
// ============================================================================
// Manages type instances to ensure type uniqueness (e.g., only one int32 type)

class TypeSystem {
private:
    std::vector<std::unique_ptr<Type>> types;  // Owns all types
    std::unordered_map<std::string, PrimitiveType*> primitiveCache;
    UnknownType* unknownType;
    ErrorType* errorType;
    
public:
    TypeSystem();
    
    // Get or create primitive type
    PrimitiveType* getPrimitiveType(const std::string& name);
    
    // Special types
    UnknownType* getUnknownType() { return unknownType; }
    ErrorType* getErrorType() { return errorType; }
    
    // TODO: Phase 3.2 - Add pointer, array, function types
};

} // namespace sema
} // namespace aria

#endif // ARIA_SEMA_TYPE_H
