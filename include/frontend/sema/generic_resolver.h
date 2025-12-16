#ifndef ARIA_GENERIC_RESOLVER_H
#define ARIA_GENERIC_RESOLVER_H

#include "frontend/ast/ast_node.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/expr.h"
#include "frontend/sema/type.h"
#include "frontend/sema/symbol_table.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>

// Forward declarations
namespace aria {
    struct GenericParamInfo;  // Defined in frontend/ast/stmt.h
}

namespace aria {
namespace sema {

// ============================================================================
// Generic Type System
// ============================================================================
// This module implements Aria's zero-cost generic system via monomorphization.
// Based on research_027_generics_templates.txt
//
// Key Features:
// - Compile-time monomorphization (like C++/Rust templates)
// - Lazy instantiation on demand
// - Bidirectional type inference
// - Trait-based constraints
// - Deep integration with TBB types and hybrid memory model
//
// Syntax: func<T, U>:name = *T(*U:param) { ... }
// - <T, U> declares type parameters
// - *T, *U references them in body (explicit sigil)
// ============================================================================

/**
 * Represents a single generic type parameter
 */
struct GenericParam {
    std::string name;                    // e.g., "T"
    std::vector<std::string> constraints; // Trait bounds: "T: Addable & Display"
    int line;
    int column;
    
    GenericParam(const std::string& n, int l = 0, int c = 0)
        : name(n), line(l), column(c) {}
    
    bool hasConstraints() const { return !constraints.empty(); }
};

/**
 * Represents a type argument provided at a call site
 */
struct TypeArg {
    Type* type;            // The concrete type
    std::string source;    // "inferred" or "explicit"
    int line;
    int column;
    
    TypeArg(Type* t, const std::string& src = "inferred", int l = 0, int c = 0)
        : type(t), source(src), line(l), column(c) {}
};

/**
 * Represents a mapping from type parameters to concrete types
 * Example: {T -> int32, U -> string}
 */
using TypeSubstitution = std::unordered_map<std::string, Type*>;

/**
 * Cache key for specialized functions
 * Format: (function_name, [type1, type2, ...])
 */
struct SpecializationKey {
    std::string funcName;
    std::vector<std::string> typeNames;  // Canonical type names
    
    bool operator==(const SpecializationKey& other) const {
        return funcName == other.funcName && typeNames == other.typeNames;
    }
};

// Hash function for SpecializationKey
struct SpecializationKeyHash {
    std::size_t operator()(const SpecializationKey& key) const {
        std::size_t h = std::hash<std::string>{}(key.funcName);
        for (const auto& typeName : key.typeNames) {
            h ^= std::hash<std::string>{}(typeName) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

/**
 * Stores a specialized (monomorphized) version of a generic function
 */
struct Specialization {
    FuncDeclStmt* funcDecl;        // The specialized AST
    std::string mangledName;       // Unique symbol name
    TypeSubstitution substitution; // Type parameter bindings
    bool analyzed;                 // Has type checking been performed?
    
    Specialization(FuncDeclStmt* decl, const std::string& name,
                   const TypeSubstitution& sub)
        : funcDecl(decl), mangledName(name), substitution(sub), analyzed(false) {}
};

/**
 * Error reported during generic resolution
 */
struct GenericError {
    int line;
    int column;
    std::string message;
    std::string context;  // Additional context information
    
    GenericError(const std::string& msg, int l = 0, int c = 0,
                 const std::string& ctx = "")
        : line(l), column(c), message(msg), context(ctx) {}
};

// ============================================================================
// GenericResolver: Type Inference and Validation
// ============================================================================

class GenericResolver {
public:
    GenericResolver();
    ~GenericResolver() = default;
    
    // ========================================================================
    // Type Inference (Phase 3.4.1)
    // ========================================================================
    
    /**
     * Infer type arguments from a function call
     * 
     * Uses bidirectional type inference:
     * 1. Generate constraints from arguments
     * 2. Unify to solve for type parameters
     * 3. Validate and return substitution
     * 
     * @param funcDecl The generic function being called
     * @param callExpr The call expression
     * @param argTypes The types of the arguments
     * @return TypeSubstitution mapping type params to concrete types
     */
    TypeSubstitution inferTypeArgs(FuncDeclStmt* funcDecl,
                                   CallExpr* callExpr,
                                   const std::vector<Type*>& argTypes);
    
    /**
     * Explicitly resolve type arguments from turbofish syntax
     * Example: identity::<int32>(42)
     * 
     * @param funcDecl The generic function
     * @param typeArgs The explicit type arguments
     * @return TypeSubstitution
     */
    TypeSubstitution resolveExplicitTypeArgs(FuncDeclStmt* funcDecl,
                                            const std::vector<std::string>& typeArgs);
    
    /**
     * Validate that a type substitution is complete and valid
     * 
     * @param funcDecl The generic function
     * @param substitution The type parameter bindings
     * @return true if valid
     */
    bool validateSubstitution(FuncDeclStmt* funcDecl,
                             const TypeSubstitution& substitution);
    
    // ========================================================================
    // Constraint Checking (Phase 3.4.3)
    // ========================================================================
    
    /**
     * Check that a concrete type satisfies the constraints on a type parameter
     * 
     * Example: If T: Addable, verify that int32 implements Addable
     * 
     * @param param The generic parameter with constraints
     * @param concreteType The type being checked
     * @return true if all constraints satisfied
     */
    bool checkConstraints(const GenericParamInfo& param, Type* concreteType);
    
    /**
     * Validate that all type parameters' constraints are satisfied
     * 
     * @param genericParams The list of parameters with constraints
     * @param substitution The concrete type bindings
     * @return true if all constraints satisfied
     */
    bool validateConstraints(const std::vector<GenericParamInfo>& genericParams,
                            const TypeSubstitution& substitution);
    
    // ========================================================================
    // Utilities
    // ========================================================================
    
    /**
     * Canonicalize a type name for cache key generation
     * Resolves aliases: type MyInt = int32 -> "int32"
     */
    std::string canonicalizeTypeName(Type* type) const;
    
    /**
     * Create a specialization key for caching
     */
    SpecializationKey makeSpecializationKey(const std::string& funcName,
                                           const TypeSubstitution& substitution) const;
    
    // Error handling
    bool hasErrors() const { return !errors.empty(); }
    const std::vector<GenericError>& getErrors() const { return errors; }
    void clearErrors() { errors.clear(); }
    
private:
    std::vector<GenericError> errors;
    
    // Helper: Add an error
    void addError(const std::string& message, int line = 0, int column = 0,
                  const std::string& context = "");
    
    // Helper: Unify two types during inference
    bool unifyTypes(Type* expected, Type* actual,
                   TypeSubstitution& substitution,
                   const std::string& paramName);
    
    // Helper: Check if a type implements a trait
    bool implementsTrait(Type* type, const std::string& traitName);
};

// ============================================================================
// Monomorphizer: Specialization Engine (Phase 3.4.2)
// ============================================================================

class Monomorphizer {
public:
    Monomorphizer(GenericResolver* resolver);
    ~Monomorphizer() = default;
    
    /**
     * Request a specialized version of a generic function
     * 
     * This is the main entry point for the monomorphization engine:
     * 1. Check cache for existing specialization
     * 2. If not found, create new specialized copy
     * 3. Perform type substitution on the cloned AST
     * 4. Return the specialized function
     * 
     * @param funcDecl The generic function template
     * @param substitution Type parameter bindings
     * @return Specialized function (cached or newly created)
     */
    Specialization* requestSpecialization(FuncDeclStmt* funcDecl,
                                         const TypeSubstitution& substitution);
    
    /**
     * Generate a mangled name for a specialized function
     * 
     * Format: _Aria_M_<FuncName>_<TypeHash>_<TypeDesc>
     * Example: _Aria_M_identity_F4A19C88_int32
     * 
     * @param funcName The base function name
     * @param substitution The type bindings
     * @return Mangled symbol name
     */
    std::string mangleName(const std::string& funcName,
                          const TypeSubstitution& substitution) const;
    
    /**
     * Clone a generic function AST and substitute types
     * 
     * Creates a deep copy of the AST, replacing all *T references
     * with the concrete type from the substitution
     * 
     * @param funcDecl The generic function to clone
     * @param substitution Type parameter bindings
     * @return New specialized FuncDeclStmt
     */
    FuncDeclStmt* cloneAndSubstitute(FuncDeclStmt* funcDecl,
                                     const TypeSubstitution& substitution);
    
    /**
     * Get all specializations created so far
     * Used for code generation phase
     */
    const std::vector<Specialization*>& getSpecializations() const {
        return specializations;
    }
    
    /**
     * Check if instantiation depth limit exceeded
     * Prevents infinite recursive instantiation
     */
    bool checkDepthLimit() const;
    
    /**
     * Get current instantiation stack depth (for testing)
     */
    size_t getInstantiationDepth() const {
        return instantiationStack.size();
    }
    
    // Error handling
    bool hasErrors() const { return !errors.empty(); }
    const std::vector<GenericError>& getErrors() const { return errors; }
    
    // Public for testing: Deep clone an AST node
    ASTNodePtr cloneAST(ASTNode* node);
    
    // Public for testing: Substitute types in an AST node
    void substituteTypes(ASTNode* node, const TypeSubstitution& substitution);
    
private:
    GenericResolver* resolver;
    
    // Cache: (function_name, type_args) -> Specialization
    std::unordered_map<SpecializationKey, Specialization*,
                       SpecializationKeyHash> specializationCache;
    
    // All specializations created (for code generation)
    std::vector<Specialization*> specializations;
    
    // Instantiation stack (for cycle detection)
    std::vector<SpecializationKey> instantiationStack;
    
    // Errors
    std::vector<GenericError> errors;
    
    // Configuration
    static const int MAX_INSTANTIATION_DEPTH = 64;
    
    // Helper: Add an error
    void addError(const std::string& message, int line = 0, int column = 0);
    
    // Helper: Compute hash for mangling
    uint64_t computeTypeHash(const TypeSubstitution& substitution) const;
};

} // namespace sema
} // namespace aria

#endif // ARIA_GENERIC_RESOLVER_H
