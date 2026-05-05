#ifndef ARIA_SEMA_TYPE_H
#define ARIA_SEMA_TYPE_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>

namespace npk {
namespace sema {

// Forward declarations
class Type;
class TypeSystem;

// ============================================================================
// Type - Base class for all types in the semantic analyzer
// ============================================================================
// Reference: research_014 (composite types), research_015 (vectors, structs),
//            research_016 (functional types)

enum class TypeKind {
    PRIMITIVE,      // int8, int32, bool, string, etc.
    POINTER,        // T@ (references)
    ARRAY,          // T[], T[N]
    SLICE,          // T[] (view into array)
    FUNCTION,       // func(params) -> return
    STRUCT,         // struct { fields }
    UNION,          // union { variants }
    VECTOR,         // vec2, vec3, vec4, etc.
    GENERIC,        // T, U, V (type parameters)
    OPTIONAL,       // T? (optional types, can be NIL)
    RESULT,         // result<T> for error handling
    FUTURE,         // future<T> for async computation
    DIMENSIONAL,    // T<Dimension> for dimensional analysis (P1-5)
    HANDLE,         // Handle<T> for generational memory handles (P1-3)
    SIMD,           // simd<T, N> for SIMD vectorization (P1-2)
    ANY,            // any - type-erased pointer (safe void* replacement)
    DYN_TRAIT,      // dyn Trait - dynamic trait object (fat pointer: data + vtable)
    ENUM,           // enum - named integer constants with type safety (v0.2.39)
    UNKNOWN,        // Type not yet inferred
    ERROR,          // Type error occurred
};

class Type {
protected:
    TypeKind kind;
    bool nodiscard;  // True if this type's value must be used (e.g., result types)
    
public:
    explicit Type(TypeKind kind, bool nodiscard = false) 
        : kind(kind), nodiscard(nodiscard) {}
    virtual ~Type() = default;
    
    TypeKind getKind() const { return kind; }
    
    // Type operations
    virtual bool equals(const Type* other) const = 0;
    virtual bool isAssignableTo(const Type* target) const = 0;
    virtual std::string toString() const = 0;
    
    // Type properties
    virtual bool isPrimitive() const { return kind == TypeKind::PRIMITIVE; }
    virtual bool isPointer() const { return kind == TypeKind::POINTER; }
    virtual bool isArray() const { return kind == TypeKind::ARRAY; }
    virtual bool isFunction() const { return kind == TypeKind::FUNCTION; }
    virtual bool isStruct() const { return kind == TypeKind::STRUCT; }
    virtual bool isVector() const { return kind == TypeKind::VECTOR; }
    virtual bool isGeneric() const { return kind == TypeKind::GENERIC; }
    virtual bool isOptional() const { return kind == TypeKind::OPTIONAL; }
    virtual bool isHandle() const { return kind == TypeKind::HANDLE; }
    virtual bool isAny() const { return kind == TypeKind::ANY; }
    virtual bool isDynTrait() const { return kind == TypeKind::DYN_TRAIT; }
    virtual bool isEnum() const { return kind == TypeKind::ENUM; }
    
    // Must-use checking (Phase 2.1 - research_011)
    virtual bool isNodiscard() const { return nodiscard; }
    virtual void setNodiscard(bool value) { nodiscard = value; }
};

// ============================================================================
// PrimitiveType - Built-in primitive types
// ============================================================================
// Reference: research_012 (integers), research_013 (floats), research_002 (TBB)

class PrimitiveType : public Type {
private:
    std::string name;  // "int8", "int32", "bool", "string", etc.
    int bitWidth;      // Bit width (8, 16, 32, 64, etc.) - 0 for non-numeric types
    bool isSigned;     // True for signed integers, false for unsigned
    bool isFloating;   // True for floating-point types
    bool isTBB;        // True for Twisted Balanced Binary types (tbb8, tbb16, etc.)
    bool isExotic;     // True for Exotic Balanced types (trit, tryte, nit, nyte)
    
public:
    explicit PrimitiveType(const std::string& name, int bitWidth = 0,
                          bool isSigned = false, bool isFloating = false, bool isTBB = false, bool isExotic = false)
        : Type(TypeKind::PRIMITIVE), name(name), bitWidth(bitWidth),
          isSigned(isSigned), isFloating(isFloating), isTBB(isTBB), isExotic(isExotic) {}
    
    const std::string& getName() const { return name; }
    int getBitWidth() const { return bitWidth; }
    bool isSignedType() const { return isSigned; }
    bool isFloatingType() const { return isFloating; }
    bool isTBBType() const { return isTBB; }
    bool isExoticType() const { return isExotic; }
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override { return name; }
};

// ============================================================================
// PointerType - Reference types (T@)
// ============================================================================
// Reference: research_001 (borrow checker), research_022 (wild pointers)

class PointerType : public Type {
private:
    Type* pointeeType;  // Type being pointed to (nullptr when isErased=true)
    bool isMutable;     // True for mutable references
    bool isWild;        // True for wild (unsafe) pointers
    bool isErased;      // True for ?-> / ?* (type-erased pointer, C's void*)
    
public:
    PointerType(Type* pointeeType, bool isMutable = false, bool isWild = false, bool isErased = false)
        : Type(TypeKind::POINTER), pointeeType(pointeeType),
          isMutable(isMutable), isWild(isWild), isErased(isErased) {}
    
    Type* getPointeeType() const { return pointeeType; }
    bool isMutableRef() const { return isMutable; }
    bool isWildPointer() const { return isWild; }
    bool isErasedPointer() const { return isErased; }
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override;
};

// ============================================================================
// ArrayType - Fixed-size arrays (T[N])
// ============================================================================
// Reference: research_016 (arrays and slices)

class ArrayType : public Type {
private:
    Type* elementType;  // Element type
    int size;           // Array size (-1 for dynamic/unknown size)
    
public:
    ArrayType(Type* elementType, int size)
        : Type(TypeKind::ARRAY), elementType(elementType), size(size) {}
    
    Type* getElementType() const { return elementType; }
    int getSize() const { return size; }
    bool isFixedSize() const { return size >= 0; }
    bool isDynamic() const { return size < 0; }
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override;
};

// ============================================================================
// VectorType - SIMD vector types (vec2, vec3, vec4, etc.)
// ============================================================================
// Reference: research_015 (vector types), research_014 (composite types part 1)

class VectorType : public Type {
private:
    Type* componentType;  // Component type (flt32, flt64, int32, etc.)
    int dimension;        // Number of components (2, 3, 4, 9, etc.)
    
public:
    VectorType(Type* componentType, int dimension)
        : Type(TypeKind::VECTOR), componentType(componentType), dimension(dimension) {}
    
    Type* getComponentType() const { return componentType; }
    int getDimension() const { return dimension; }
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override;
};

// ============================================================================
// FunctionType - Function signatures (func(params) -> return)
// ============================================================================
// Reference: research_016 (functional types)

class FunctionType : public Type {
private:
    std::vector<Type*> paramTypes;  // Parameter types
    Type* returnType;                // Return type
    bool isAsync;                    // True for async functions
    bool isVariadic;                 // True for variadic functions (...)
    
public:
    FunctionType(const std::vector<Type*>& paramTypes, Type* returnType,
                bool isAsync = false, bool isVariadic = false)
        : Type(TypeKind::FUNCTION), paramTypes(paramTypes), returnType(returnType),
          isAsync(isAsync), isVariadic(isVariadic) {}
    
    const std::vector<Type*>& getParamTypes() const { return paramTypes; }
    Type* getReturnType() const { return returnType; }
    bool isAsyncFunction() const { return isAsync; }
    bool isVariadicFunction() const { return isVariadic; }
    int getParamCount() const { return paramTypes.size(); }
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override;
};

// ============================================================================
// StructType - Struct definitions
// ============================================================================
// Reference: research_015 (struct types and layout)

class StructType : public Type {
public:
    struct Field {
        std::string name;
        Type* type;
        int offset;      // Byte offset in struct layout
        bool isPublic;   // Visibility
        
        Field(const std::string& name, Type* type, int offset = 0, bool isPublic = false)
            : name(name), type(type), offset(offset), isPublic(isPublic) {}
    };
    
private:
    std::string name;            // Struct name
    std::vector<Field> fields;   // Field definitions
    int size;                    // Total size in bytes
    int alignment;               // Alignment requirement
    bool isPacked;               // True if @pack directive used
    
public:
    StructType(const std::string& name, const std::vector<Field>& fields,
              int size = 0, int alignment = 0, bool isPacked = false)
        : Type(TypeKind::STRUCT), name(name), fields(fields),
          size(size), alignment(alignment), isPacked(isPacked) {}
    
    const std::string& getName() const { return name; }
    const std::vector<Field>& getFields() const { return fields; }
    int getSize() const { return size; }
    int getAlignment() const { return alignment; }
    bool isPackedStruct() const { return isPacked; }
    
    // Field lookup
    const Field* getField(const std::string& fieldName) const;
    int getFieldIndex(const std::string& fieldName) const;
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override;
};

// ============================================================================
// UnionType - Union definitions
// ============================================================================

class UnionType : public Type {
public:
    struct Variant {
        std::string name;
        Type* type;
        
        Variant(const std::string& name, Type* type)
            : name(name), type(type) {}
    };
    
private:
    std::string name;              // Union name
    std::vector<Variant> variants; // Variant definitions
    int size;                      // Total size (max of all variants)
    
public:
    UnionType(const std::string& name, const std::vector<Variant>& variants, int size = 0)
        : Type(TypeKind::UNION), name(name), variants(variants), size(size) {}
    
    const std::string& getName() const { return name; }
    const std::vector<Variant>& getVariants() const { return variants; }
    int getSize() const { return size; }
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override;
};

// ============================================================================
// GenericType - Type parameters (T, U, V)
// ============================================================================
// Reference: research_027 (generics and templates)

class GenericType : public Type {
private:
    std::string name;  // Type parameter name (T, U, V, etc.)
    
public:
    explicit GenericType(const std::string& name)
        : Type(TypeKind::GENERIC), name(name) {}
    
    const std::string& getName() const { return name; }
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override { return name; }
};

// ============================================================================
// Dimension - Represents physical dimensions for dimensional analysis (P1-5)
// ============================================================================
// Physical dimensions represented as exponents over SI base dimensions.
// Example: Joules = kg⋅m²⋅s⁻² = {mass:1, length:2, time:-2, ...}

struct Dimension {
    int8_t length;       // meters (m)
    int8_t mass;         // kilograms (kg)
    int8_t time;         // seconds (s)
    int8_t current;      // amperes (A)
    int8_t temperature;  // kelvin (K)
    int8_t amount;       // moles (mol)
    int8_t luminosity;   // candela (cd)
    
    // Constructor for dimensionless
    Dimension()
        : length(0), mass(0), time(0), current(0), 
          temperature(0), amount(0), luminosity(0) {}
    
    // Constructor for explicit dimensions
    Dimension(int8_t len, int8_t m, int8_t t, int8_t i, int8_t temp, int8_t amt, int8_t lum)
        : length(len), mass(m), time(t), current(i), 
          temperature(temp), amount(amt), luminosity(lum) {}
    
    // Named dimension constructors
    static Dimension Dimensionless() { return Dimension(); }
    static Dimension Length() { return Dimension(1, 0, 0, 0, 0, 0, 0); }
    static Dimension Mass() { return Dimension(0, 1, 0, 0, 0, 0, 0); }
    static Dimension Time() { return Dimension(0, 0, 1, 0, 0, 0, 0); }
    static Dimension Current() { return Dimension(0, 0, 0, 1, 0, 0, 0); }
    static Dimension Temperature() { return Dimension(0, 0, 0, 0, 1, 0, 0); }
    static Dimension Amount() { return Dimension(0, 0, 0, 0, 0, 1, 0); }
    static Dimension Luminosity() { return Dimension(0, 0, 0, 0, 0, 0, 1); }
    
    // Common derived dimensions
    static Dimension Velocity() { return Dimension(1, 0, -1, 0, 0, 0, 0); }  // m/s
    static Dimension Acceleration() { return Dimension(1, 0, -2, 0, 0, 0, 0); }  // m/s²
    static Dimension Force() { return Dimension(1, 1, -2, 0, 0, 0, 0); }  // N = kg⋅m/s²
    static Dimension Energy() { return Dimension(2, 1, -2, 0, 0, 0, 0); }  // J = kg⋅m²/s²
    static Dimension Power() { return Dimension(2, 1, -3, 0, 0, 0, 0); }  // W = kg⋅m²/s³
    static Dimension Charge() { return Dimension(0, 0, 1, 1, 0, 0, 0); }  // C = A⋅s
    static Dimension Voltage() { return Dimension(2, 1, -3, -1, 0, 0, 0); }  // V = kg⋅m²/(A⋅s³)
    static Dimension Resistance() { return Dimension(2, 1, -3, -2, 0, 0, 0); }  // Ω = kg⋅m²/(A²⋅s³)
    static Dimension Frequency() { return Dimension(0, 0, -1, 0, 0, 0, 0); }  // Hz = 1/s
    static Dimension Action() { return Dimension(2, 1, -1, 0, 0, 0, 0); }  // J⋅s = kg⋅m²/s
    
    // Dimensional algebra
    Dimension operator*(const Dimension& other) const {
        return Dimension(
            length + other.length,
            mass + other.mass,
            time + other.time,
            current + other.current,
            temperature + other.temperature,
            amount + other.amount,
            luminosity + other.luminosity
        );
    }
    
    Dimension operator/(const Dimension& other) const {
        return Dimension(
            length - other.length,
            mass - other.mass,
            time - other.time,
            current - other.current,
            temperature - other.temperature,
            amount - other.amount,
            luminosity - other.luminosity
        );
    }
    
    bool operator==(const Dimension& other) const {
        return length == other.length &&
               mass == other.mass &&
               time == other.time &&
               current == other.current &&
               temperature == other.temperature &&
               amount == other.amount &&
               luminosity == other.luminosity;
    }
    
    bool operator!=(const Dimension& other) const {
        return !(*this == other);
    }
    
    bool isDimensionless() const {
        return length == 0 && mass == 0 && time == 0 && current == 0 &&
               temperature == 0 && amount == 0 && luminosity == 0;
    }
    
    std::string toString() const;
};

// ============================================================================
// DimensionalType - Numeric types with physical dimensions (P1-5)
// ============================================================================
// Wraps a base numeric type with dimension information for compile-time
// dimensional analysis. Example: fix256<Joules>, fix256<Meters>

class DimensionalType : public Type {
private:
    Type* baseType;        // Underlying numeric type (fix256, flt64, etc.)
    Dimension dimension;   // Physical dimension
    std::string dimensionName;  // Optional name (Joules, Meters, etc.)
    
public:
    DimensionalType(Type* baseType, const Dimension& dimension, 
                   const std::string& dimensionName = "")
        : Type(TypeKind::DIMENSIONAL), baseType(baseType), 
          dimension(dimension), dimensionName(dimensionName) {}
    
    Type* getBaseType() const { return baseType; }
    const Dimension& getDimension() const { return dimension; }
    const std::string& getDimensionName() const { return dimensionName; }
    
    bool isDimensionless() const { return dimension.isDimensionless(); }
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override;
};

// ============================================================================
// OptionalType - Optional type (T?)
// ============================================================================
// Reference: aria_specs.txt (NIL and optional types)
// Represents a type that can be either a value of type T or NIL

class OptionalType : public Type {
private:
    Type* wrappedType;  // The underlying type (T in T?)
    
public:
    explicit OptionalType(Type* wrappedType)
        : Type(TypeKind::OPTIONAL), wrappedType(wrappedType) {}
    
    Type* getWrappedType() const { return wrappedType; }
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override;
};

// ============================================================================
// ResultType - Result type for error handling (result<T>)
// ============================================================================
// Reference: research_016 (result type and error propagation)
// ResultType is ALWAYS nodiscard - unused results are a bug!

class ResultType : public Type {
private:
    Type* valueType;  // Success value type
    
public:
    explicit ResultType(Type* valueType)
        : Type(TypeKind::RESULT, true), valueType(valueType) {}  // nodiscard = true
    
    Type* getValueType() const { return valueType; }
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override;
};

// ============================================================================
// FutureType - Future type for async computation (future<T>)
// ============================================================================
// Reference: research_029 (async/await system), section 4 (Future trait design)

class FutureType : public Type {
private:
    Type* outputType;  // The type produced when the Future completes
    
public:
    explicit FutureType(Type* outputType)
        : Type(TypeKind::FUTURE), outputType(outputType) {}
    
    Type* getOutputType() const { return outputType; }
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override;
};

// ============================================================================
// HandleType - Generational memory handle (Handle<T>)
// ============================================================================
// Reference: P1-3 (Generational Handles for neurogenesis safety)
// Handles provide stable references across arena reallocation
// Handle = {index: usize, generation: u32}

class HandleType : public Type {
private:
    Type* pointeeType;  // The type being handled (T in Handle<T>)
    
public:
    explicit HandleType(Type* pointeeType)
        : Type(TypeKind::HANDLE), pointeeType(pointeeType) {}
    
    Type* getPointeeType() const { return pointeeType; }
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override;
};

// ============================================================================
// SimdType - SIMD vector type (simd<T, N>)
// ============================================================================
// Reference: P1-2 (SIMD Vectorization for performance)
// SIMD types represent vector registers for explicit vectorization
// simd<T, N> = N lanes of type T (e.g., simd<fix256, 16> for AVX-512)

class SimdType : public Type {
private:
    Type* elementType;   // Element type (T in simd<T, N>)
    size_t laneCount;    // Number of SIMD lanes (N in simd<T, N>)
    
public:
    SimdType(Type* elementType, size_t laneCount)
        : Type(TypeKind::SIMD), elementType(elementType), laneCount(laneCount) {}
    
    Type* getElementType() const { return elementType; }
    size_t getLaneCount() const { return laneCount; }
    
    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override;
};

// ============================================================================
// AnyType - Type-erased pointer (safe void* replacement)
// ============================================================================
// any is a first-class type-erased pointer type. It holds an opaque pointer
// to data of unknown type, with runtime-checked get/set and consuming resolve.
// - get<T>(): Read as T (runtime checked)
// - set<T>(val): Write T value (runtime checked)
// - resolve<T>(): Permanently transforms any into T-> (consuming/move)
// - wild any: Opts out of runtime checks (FFI/JIT interop)

class AnyType : public Type {
private:
    bool isWild;     // True for wild any (no runtime checks)
    bool isWildx;    // True for wildx any (executable memory)

public:
    explicit AnyType(bool isWild = false, bool isWildx = false)
        : Type(TypeKind::ANY), isWild(isWild), isWildx(isWildx) {}

    bool isWildAny() const { return isWild; }
    bool isWildxAny() const { return isWildx; }

    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override;
};

// ============================================================================
// DynTraitType - Dynamic trait object (dyn Trait)
// ============================================================================
// A fat pointer containing {data_ptr, vtable_ptr}.
// data_ptr: points to the concrete value
// vtable_ptr: points to the trait's vtable for this concrete type
// Used for runtime polymorphism (heterogeneous collections, plugin dispatch).

class DynTraitType : public Type {
private:
    std::string traitName;  // Name of the trait (e.g., "Encodable")

public:
    explicit DynTraitType(const std::string& traitName)
        : Type(TypeKind::DYN_TRAIT), traitName(traitName) {}

    const std::string& getTraitName() const { return traitName; }

    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override;
};

// ============================================================================
// EnumType - Named integer constants with type safety (v0.2.39)
// ============================================================================
// enum:Color = { RED, GREEN, BLUE };
// Stored as i64 at LLVM level but type-checked at sema level.
// Variables: Color:c = Color.RED;
// Exhaustive pick matching on enum types.

class EnumType : public Type {
private:
    std::string name;                           // Enum name (e.g., "Color")
    std::map<std::string, int64_t> variants;    // Variant name -> integer value

public:
    EnumType(const std::string& name, const std::map<std::string, int64_t>& variants)
        : Type(TypeKind::ENUM), name(name), variants(variants) {}

    const std::string& getName() const { return name; }
    const std::map<std::string, int64_t>& getVariants() const { return variants; }

    // Check if a variant name exists in this enum
    bool hasVariant(const std::string& variantName) const {
        return variants.find(variantName) != variants.end();
    }

    // Get variant value (-1 if not found, but callers should use hasVariant first)
    int64_t getVariantValue(const std::string& variantName) const {
        auto it = variants.find(variantName);
        return it != variants.end() ? it->second : -1;
    }

    bool equals(const Type* other) const override;
    bool isAssignableTo(const Type* target) const override;
    std::string toString() const override;
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
// Manages type instances to ensure type uniqueness

class TypeSystem {
private:
    std::vector<std::unique_ptr<Type>> types;  // Owns all types
    std::unordered_map<std::string, PrimitiveType*> primitiveCache;
    std::unordered_map<std::string, GenericType*> genericCache;
    std::unordered_map<std::string, StructType*> structCache;
    std::unordered_map<std::string, UnionType*> unionCache;
    std::unordered_map<std::string, EnumType*> enumCache;        // v0.2.39: Enum type cache
    std::unordered_map<std::string, DimensionalType*> dimensionalCache;  // P1-5
    std::unordered_map<std::string, PointerType*> pointerCache;
    std::unordered_map<std::string, ArrayType*> arrayCache;
    std::unordered_map<std::string, VectorType*> vectorCache;
    std::unordered_map<std::string, FunctionType*> functionCache;
    std::unordered_map<std::string, OptionalType*> optionalCache;
    std::unordered_map<std::string, ResultType*> resultCache;
    std::unordered_map<std::string, FutureType*> futureCache;
    std::unordered_map<std::string, HandleType*> handleCache;
    std::unordered_map<std::string, SimdType*> simdCache;
    
    // Standard dimension name registry (P1-5)
    std::unordered_map<std::string, Dimension> dimensionRegistry;
    
    UnknownType* unknownType;
    ErrorType* errorType;
    
    // Initialize dimension name registry (P1-5)
    void initializeDimensionRegistry();
    
public:
    TypeSystem();
    
    // Primitive types
    PrimitiveType* getPrimitiveType(const std::string& name);
    
    // Composite types
    PointerType* getPointerType(Type* pointeeType, bool isMutable = false, bool isWild = false);
    PointerType* getErasedPointerType(bool isMutable = false, bool isWild = false); // ARIA-P3: ?-> / ?*
    ArrayType* getArrayType(Type* elementType, int size);
    VectorType* getVectorType(Type* componentType, int dimension);
    FunctionType* getFunctionType(const std::vector<Type*>& paramTypes, Type* returnType,
                                 bool isAsync = false, bool isVariadic = false);
    OptionalType* getOptionalType(Type* wrappedType);
    ResultType* getResultType(Type* valueType);
    FutureType* getFutureType(Type* outputType);
    HandleType* getHandleType(Type* pointeeType);  // P1-3: Generational handles
    SimdType* getSimdType(Type* elementType, size_t laneCount);  // P1-2: SIMD vectorization
    AnyType* getAnyType(bool isWild = false, bool isWildx = false);  // Type-erased pointer
    DynTraitType* getDynTraitType(const std::string& traitName);     // dyn Trait fat pointer
    
    // Type extraction helpers
    // Phase 4.5.3: Extract T from future<T> for await expressions
    Type* unwrapFutureType(Type* futureType);
    
    // Named types
    StructType* getStructType(const std::string& name);
    StructType* createStructType(const std::string& name, const std::vector<StructType::Field>& fields,
                                int size = 0, int alignment = 0, bool isPacked = false);
    void registerStructAlias(const std::string& alias, StructType* type);
    
    UnionType* getUnionType(const std::string& name);
    UnionType* createUnionType(const std::string& name, const std::vector<UnionType::Variant>& variants,
                              int size = 0);
    
    // v0.2.39: Enum types
    EnumType* getEnumType(const std::string& name);
    EnumType* createEnumType(const std::string& name, const std::map<std::string, int64_t>& variants);
    
    // Generic types
    GenericType* getGenericType(const std::string& name);
    
    // Dimensional types (P1-5)
    DimensionalType* getDimensionalType(Type* baseType, const Dimension& dimension, 
                                       const std::string& dimensionName = "");
    DimensionalType* getDimensionalType(Type* baseType, const std::string& dimensionName);
    
    // Dimension registry (P1-5)
    const Dimension* lookupDimension(const std::string& name) const;
    void registerDimension(const std::string& name, const Dimension& dimension);
    
    // Special types
    UnknownType* getUnknownType() { return unknownType; }
    ErrorType* getErrorType() { return errorType; }
};

} // namespace sema
} // namespace npk

#endif // ARIA_SEMA_TYPE_H
