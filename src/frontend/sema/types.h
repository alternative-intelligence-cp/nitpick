/**
 * src/frontend/sema/types.h
 *
 * Aria Type System
 * Version: 0.0.6
 *
 * Defines the type representation and type checking infrastructure.
 */

#ifndef ARIA_FRONTEND_SEMA_TYPES_H
#define ARIA_FRONTEND_SEMA_TYPES_H

#include <string>
#include <memory>
#include <map>
#include <vector>

namespace aria {
namespace sema {

// Type kinds in Aria
enum class TypeKind {
    VOID,
    BOOL,
    INT8, INT16, INT32, INT64, INT128, INT256, INT512,
    UINT8, UINT16, UINT32, UINT64,
    FLT32, FLT64,
    TRIT, TRYTE,  // Ternary types
    NIT, NYTE,     // Nonary types
    STRING,
    DYN,           // Dynamic type (GC-allocated catch-all)
    POINTER,       // Wild or pinned pointer
    ARRAY,
    FUNCTION,
    STRUCT,
    FUTURE,        // Future<T> - async result handle
    // SIMD Vector types for hardware-accelerated operations
    VEC2, VEC3, VEC4,     // Float vectors (32-bit)
    DVEC2, DVEC3, DVEC4,  // Double vectors (64-bit)
    IVEC2, IVEC3, IVEC4,  // Integer vectors (32-bit)
    UNKNOWN,       // For type inference errors
    ERROR          // Type error marker
};

// Type representation
class Type {
public:
    TypeKind kind;
    std::string name;

    // For pointers
    bool is_wild = false;
    bool is_pinned = false;
    std::shared_ptr<Type> pointee = nullptr;

    // For arrays
    std::shared_ptr<Type> element_type = nullptr;
    int array_size = -1;  // -1 for dynamic arrays

    // For functions
    std::shared_ptr<Type> return_type = nullptr;
    std::vector<std::shared_ptr<Type>> param_types;
    
    // For Future<T>
    std::shared_ptr<Type> future_value_type = nullptr;

    Type(TypeKind k, const std::string& n = "") : kind(k), name(n) {}

    // Check if two types are equal
    bool equals(const Type& other) const {
        if (kind != other.kind) return false;

        if (kind == TypeKind::POINTER) {
            if (is_wild != other.is_wild) return false;
            if (is_pinned != other.is_pinned) return false;
            if (pointee && other.pointee) {
                return pointee->equals(*other.pointee);
            }
            return pointee == other.pointee;
        }

        if (kind == TypeKind::ARRAY) {
            if (array_size != other.array_size) return false;
            if (element_type && other.element_type) {
                return element_type->equals(*other.element_type);
            }
            return element_type == other.element_type;
        }
        
        if (kind == TypeKind::FUTURE) {
            if (future_value_type && other.future_value_type) {
                return future_value_type->equals(*other.future_value_type);
            }
            return future_value_type == other.future_value_type;
        }

        return true;
    }

    // String representation
    std::string toString() const {
        if (!name.empty()) return name;

        switch (kind) {
            case TypeKind::VOID: return "void";
            case TypeKind::BOOL: return "bool";
            case TypeKind::INT8: return "int8";
            case TypeKind::INT16: return "int16";
            case TypeKind::INT32: return "int32";
            case TypeKind::INT64: return "int64";
            case TypeKind::INT128: return "int128";
            case TypeKind::INT256: return "int256";
            case TypeKind::INT512: return "int512";
            case TypeKind::FLT32: return "flt32";
            case TypeKind::FLT64: return "flt64";
            case TypeKind::STRING: return "string";
            case TypeKind::DYN: return "dyn";
            case TypeKind::TRIT: return "trit";
            case TypeKind::TRYTE: return "tryte";
            case TypeKind::VEC2: return "vec2";
            case TypeKind::VEC3: return "vec3";
            case TypeKind::VEC4: return "vec4";
            case TypeKind::DVEC2: return "dvec2";
            case TypeKind::DVEC3: return "dvec3";
            case TypeKind::DVEC4: return "dvec4";
            case TypeKind::IVEC2: return "ivec2";
            case TypeKind::IVEC3: return "ivec3";
            case TypeKind::IVEC4: return "ivec4";
            case TypeKind::POINTER:
                if (pointee) {
                    std::string prefix = is_wild ? "wild " : (is_pinned ? "pinned " : "");
                    return prefix + pointee->toString() + "*";
                }
                return "void*";
            case TypeKind::ARRAY:
                if (element_type) {
                    return element_type->toString() + "[" +
                           (array_size >= 0 ? std::to_string(array_size) : "") + "]";
                }
                return "array";
            case TypeKind::FUTURE:
                if (future_value_type) {
                    return "Future<" + future_value_type->toString() + ">";
                }
                return "Future";
            case TypeKind::FUNCTION: return "func";
            case TypeKind::UNKNOWN: return "unknown";
            case TypeKind::ERROR: return "<error>";
            default: return "<type>";
        }
    }

    // Check if type is numeric
    bool isNumeric() const {
        return kind == TypeKind::INT8 || kind == TypeKind::INT16 ||
               kind == TypeKind::INT32 || kind == TypeKind::INT64 ||
               kind == TypeKind::INT128 || kind == TypeKind::INT256 ||
               kind == TypeKind::INT512 || kind == TypeKind::FLT32 ||
               kind == TypeKind::FLT64;
    }

    // Check if type is integer
    bool isInteger() const {
        return kind == TypeKind::INT8 || kind == TypeKind::INT16 ||
               kind == TypeKind::INT32 || kind == TypeKind::INT64 ||
               kind == TypeKind::INT128 || kind == TypeKind::INT256 ||
               kind == TypeKind::INT512;
    }

    // Check if type is floating point
    bool isFloat() const {
        return kind == TypeKind::FLT32 || kind == TypeKind::FLT64;
    }
};

// Symbol table entry
struct Symbol {
    std::string name;
    std::shared_ptr<Type> type;
    std::string type_name;  // String representation of type (for capture analysis)
    bool is_mutable = false;
    bool is_initialized = false;
    int scope_level = 0;
    
    // Function signature info (if this symbol is a function)
    bool is_function = false;
    std::string function_return_type;  // Return type string (e.g., "int8")
    std::vector<std::string> function_param_types;  // Parameter type strings
};

// Symbol table for type checking
class SymbolTable {
private:
    std::map<std::string, Symbol> symbols;
    std::unique_ptr<SymbolTable> parent;
    int current_scope_level = 0;

public:
    SymbolTable() = default;
    SymbolTable(std::unique_ptr<SymbolTable> p) : parent(std::move(p)) {
        if (parent) {
            current_scope_level = parent->current_scope_level + 1;
        }
    }

    // Define a symbol in the current scope
    bool define(const std::string& name, std::shared_ptr<Type> type, bool is_mut = false) {
        // Check if already defined in current scope
        if (symbols.count(name) > 0 && symbols[name].scope_level == current_scope_level) {
            return false;  // Redefinition error
        }

        Symbol sym;
        sym.name = name;
        sym.type = type;
        sym.type_name = type->toString();  // Store string representation
        sym.is_mutable = is_mut;
        sym.is_initialized = true;
        sym.scope_level = current_scope_level;

        symbols[name] = sym;
        return true;
    }

    // Lookup a symbol (searches parent scopes)
    Symbol* lookup(const std::string& name) {
        auto it = symbols.find(name);
        if (it != symbols.end()) {
            return &it->second;
        }

        if (parent) {
            return parent->lookup(name);
        }

        return nullptr;
    }
    
    // Check if a symbol is global (defined at scope level 0)
    bool isGlobal(const std::string& name) {
        auto* sym = lookup(name);
        return sym && sym->scope_level == 0;
    }

    // Note: Scope management is handled manually in type checker
    // Each new block creates a new SymbolTable with this as parent

    // Get current scope level
    int getScopeLevel() const {
        return current_scope_level;
    }
};

// Helper functions for creating types
inline std::shared_ptr<Type> makeVoidType() {
    return std::make_shared<Type>(TypeKind::VOID, "void");
}

inline std::shared_ptr<Type> makeBoolType() {
    return std::make_shared<Type>(TypeKind::BOOL, "bool");
}

inline std::shared_ptr<Type> makeIntType(int bits) {
    switch (bits) {
        case 8: return std::make_shared<Type>(TypeKind::INT8, "int8");
        case 16: return std::make_shared<Type>(TypeKind::INT16, "int16");
        case 32: return std::make_shared<Type>(TypeKind::INT32, "int32");
        case 64: return std::make_shared<Type>(TypeKind::INT64, "int64");
        case 128: return std::make_shared<Type>(TypeKind::INT128, "int128");
        case 256: return std::make_shared<Type>(TypeKind::INT256, "int256");
        case 512: return std::make_shared<Type>(TypeKind::INT512, "int512");
        default: return std::make_shared<Type>(TypeKind::INT64, "int64");
    }
}

inline std::shared_ptr<Type> makeFloatType(int bits) {
    if (bits == 32) return std::make_shared<Type>(TypeKind::FLT32, "flt32");
    return std::make_shared<Type>(TypeKind::FLT64, "flt64");
}

inline std::shared_ptr<Type> makeStringType() {
    return std::make_shared<Type>(TypeKind::STRING, "string");
}

inline std::shared_ptr<Type> makeDynType() {
    return std::make_shared<Type>(TypeKind::DYN, "dyn");
}

inline std::shared_ptr<Type> makeErrorType() {
    return std::make_shared<Type>(TypeKind::ERROR, "<error>");
}

inline std::shared_ptr<Type> makeFuncType() {
    // Generic function type - can hold any function signature
    return std::make_shared<Type>(TypeKind::FUNCTION, "func");
}

// Parse type from string (e.g., "int64", "string", "wild int32*")
std::shared_ptr<Type> parseType(const std::string& type_str);

} // namespace sema
} // namespace aria

#endif // ARIA_FRONTEND_SEMA_TYPES_H
