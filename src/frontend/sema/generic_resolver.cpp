#include "frontend/sema/generic_resolver.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/expr.h"
#include <sstream>
#include <algorithm>

namespace aria {
namespace sema {

// ============================================================================
// GenericResolver Implementation
// ============================================================================

GenericResolver::GenericResolver() {}

TypeSubstitution GenericResolver::inferTypeArgs(
    FuncDeclStmt* funcDecl,
    CallExpr* callExpr,
    const std::vector<Type*>& argTypes) {
    
    TypeSubstitution substitution;
    
    if (!funcDecl || funcDecl->genericParams.empty()) {
        addError("Function is not generic", callExpr ? callExpr->line : 0,
                callExpr ? callExpr->column : 0);
        return substitution;
    }
    
    // Check argument count
    if (funcDecl->parameters.size() != argTypes.size()) {
        addError("Argument count mismatch in generic call",
                callExpr ? callExpr->line : 0, callExpr ? callExpr->column : 0);
        return substitution;
    }
    
    // Phase 1: Generate constraints from arguments
    // For each parameter, unify its type with the corresponding argument type
    for (size_t i = 0; i < funcDecl->parameters.size(); i++) {
        ParameterNode* param = static_cast<ParameterNode*>(funcDecl->parameters[i].get());
        Type* argType = argTypes[i];
        
        // Check if parameter type contains a generic parameter
        // Format: *T means the parameter is of generic type T
        const std::string& paramTypeName = param->typeName;
        
        if (paramTypeName.empty()) continue;
        
        // Check if this is a generic type reference (*T)
        if (paramTypeName[0] == '*') {
            std::string typeParamName = paramTypeName.substr(1);
            
            // Try to unify with this type parameter
            if (!unifyTypes(nullptr, argType, substitution, typeParamName)) {
                addError("Failed to infer type parameter '" + typeParamName + "'",
                        callExpr ? callExpr->line : 0,
                        callExpr ? callExpr->column : 0,
                        "Could not unify with argument type");
                return TypeSubstitution();
            }
        }
    }
    
    // Phase 2: Validate that all type parameters have been inferred
    for (const auto& paramName : funcDecl->genericParams) {
        if (substitution.find(paramName) == substitution.end()) {
            addError("Unable to infer type parameter '" + paramName + "'",
                    callExpr ? callExpr->line : 0,
                    callExpr ? callExpr->column : 0,
                    "Type parameter appears in return type or is unused");
            return TypeSubstitution();
        }
    }
    
    return substitution;
}

TypeSubstitution GenericResolver::resolveExplicitTypeArgs(
    FuncDeclStmt* funcDecl,
    const std::vector<std::string>& typeArgs) {
    
    TypeSubstitution substitution;
    
    if (!funcDecl) {
        addError("Invalid function declaration");
        return substitution;
    }
    
    // Check count match
    if (funcDecl->genericParams.size() != typeArgs.size()) {
        addError("Type argument count mismatch: expected " +
                std::to_string(funcDecl->genericParams.size()) +
                ", got " + std::to_string(typeArgs.size()));
        return substitution;
    }
    
    // Create substitution map
    for (size_t i = 0; i < funcDecl->genericParams.size(); i++) {
        const std::string& paramName = funcDecl->genericParams[i];
        const std::string& typeName = typeArgs[i];
        
        // TODO: Resolve type name to Type* via TypeRegistry
        // For now, we'll store nullptr and resolve later
        substitution[paramName] = nullptr;
    }
    
    return substitution;
}

bool GenericResolver::validateSubstitution(
    FuncDeclStmt* funcDecl,
    const TypeSubstitution& substitution) {
    
    if (!funcDecl) return false;
    
    // Check that all type parameters have bindings
    for (const auto& paramName : funcDecl->genericParams) {
        auto it = substitution.find(paramName);
        if (it == substitution.end()) {
            addError("Missing type binding for parameter '" + paramName + "'");
            return false;
        }
        
        if (it->second == nullptr) {
            addError("Null type binding for parameter '" + paramName + "'");
            return false;
        }
    }
    
    return true;
}

bool GenericResolver::checkConstraints(const GenericParam& param, Type* concreteType) {
    if (!concreteType) return false;
    
    // If no constraints, any type is valid
    if (!param.hasConstraints()) {
        return true;
    }
    
    // Check each trait constraint
    for (const auto& traitName : param.constraints) {
        if (!implementsTrait(concreteType, traitName)) {
            addError("Type '" + concreteType->toString() +
                    "' does not satisfy trait bound '" + traitName + "'",
                    param.line, param.column);
            return false;
        }
    }
    
    return true;
}

bool GenericResolver::validateConstraints(
    const std::vector<GenericParam>& genericParams,
    const TypeSubstitution& substitution) {
    
    bool allValid = true;
    
    for (const auto& param : genericParams) {
        auto it = substitution.find(param.name);
        if (it == substitution.end()) {
            addError("No type binding for parameter '" + param.name + "'",
                    param.line, param.column);
            allValid = false;
            continue;
        }
        
        if (!checkConstraints(param, it->second)) {
            allValid = false;
        }
    }
    
    return allValid;
}

std::string GenericResolver::canonicalizeTypeName(Type* type) const {
    if (!type) return "unknown";
    
    // For now, just return the type name
    // TODO: Resolve type aliases to their underlying types
    return type->toString();
}

SpecializationKey GenericResolver::makeSpecializationKey(
    const std::string& funcName,
    const TypeSubstitution& substitution) const {
    
    SpecializationKey key;
    key.funcName = funcName;
    
    // Create sorted list of type names for consistent key
    std::vector<std::pair<std::string, Type*>> sortedBindings(
        substitution.begin(), substitution.end());
    std::sort(sortedBindings.begin(), sortedBindings.end());
    
    for (const auto& [paramName, type] : sortedBindings) {
        key.typeNames.push_back(canonicalizeTypeName(type));
    }
    
    return key;
}

void GenericResolver::addError(const std::string& message, int line, int column,
                              const std::string& context) {
    errors.emplace_back(message, line, column, context);
}

bool GenericResolver::unifyTypes(Type* expected, Type* actual,
                                TypeSubstitution& substitution,
                                const std::string& paramName) {
    
    // Check if this parameter is already bound
    auto it = substitution.find(paramName);
    if (it != substitution.end()) {
        // Parameter already bound - check for consistency
        Type* boundType = it->second;
        if (boundType->toString() != actual->toString()) {
            addError("Type parameter '" + paramName +
                    "' bound to multiple different types: '" +
                    boundType->toString() + "' and '" +
                    actual->toString() + "'");
            return false;
        }
        return true;
    }
    
    // Bind the parameter to this type
    substitution[paramName] = actual;
    return true;
}

bool GenericResolver::implementsTrait(Type* type, const std::string& traitName) {
    // TODO: Implement proper trait resolution
    // For now, we'll assume all types implement all traits
    // This will be implemented when we add the trait system
    return true;
}

// ============================================================================
// Monomorphizer Implementation
// ============================================================================

Monomorphizer::Monomorphizer(GenericResolver* resolver)
    : resolver(resolver) {}

Specialization* Monomorphizer::requestSpecialization(
    FuncDeclStmt* funcDecl,
    const TypeSubstitution& substitution) {
    
    if (!funcDecl) {
        addError("Invalid function declaration");
        return nullptr;
    }
    
    // Create cache key
    SpecializationKey key = resolver->makeSpecializationKey(
        funcDecl->funcName, substitution);
    
    // Check cache
    auto it = specializationCache.find(key);
    if (it != specializationCache.end()) {
        return it->second;
    }
    
    // Check instantiation depth
    if (instantiationStack.size() >= MAX_INSTANTIATION_DEPTH) {
        addError("Maximum generic instantiation depth exceeded (64)",
                funcDecl->line, funcDecl->column);
        return nullptr;
    }
    
    // Check for cycles
    for (const auto& stackKey : instantiationStack) {
        if (stackKey == key) {
            addError("Recursive generic instantiation detected for function '" +
                    funcDecl->funcName + "'", funcDecl->line, funcDecl->column);
            return nullptr;
        }
    }
    
    // Push onto instantiation stack
    instantiationStack.push_back(key);
    
    // Create new specialization
    FuncDeclStmt* cloned = cloneAndSubstitute(funcDecl, substitution);
    if (!cloned) {
        instantiationStack.pop_back();
        return nullptr;
    }
    
    std::string mangledName = mangleName(funcDecl->funcName, substitution);
    
    Specialization* spec = new Specialization(cloned, mangledName, substitution);
    
    // Add to cache and list
    specializationCache[key] = spec;
    specializations.push_back(spec);
    
    // Pop from instantiation stack
    instantiationStack.pop_back();
    
    return spec;
}

std::string Monomorphizer::mangleName(
    const std::string& funcName,
    const TypeSubstitution& substitution) const {
    
    std::ostringstream oss;
    oss << "_Aria_M_" << funcName;
    
    // Compute hash
    uint64_t hash = computeTypeHash(substitution);
    oss << "_" << std::hex << hash;
    
    // Add readable type description
    std::vector<std::pair<std::string, Type*>> sortedBindings(
        substitution.begin(), substitution.end());
    std::sort(sortedBindings.begin(), sortedBindings.end());
    
    for (const auto& [paramName, type] : sortedBindings) {
        oss << "_" << resolver->canonicalizeTypeName(type);
    }
    
    return oss.str();
}

FuncDeclStmt* Monomorphizer::cloneAndSubstitute(
    FuncDeclStmt* funcDecl,
    const TypeSubstitution& substitution) {
    
    if (!funcDecl) return nullptr;
    
    // Clone the function declaration
    // Note: We need to do a deep copy and substitute *T references
    
    // Clone parameters
    std::vector<ASTNodePtr> clonedParams;
    for (const auto& param : funcDecl->parameters) {
        ASTNodePtr clonedParam = cloneAST(param.get());
        if (clonedParam) {
            substituteTypes(clonedParam.get(), substitution);
            clonedParams.push_back(std::move(clonedParam));
        }
    }
    
    // Clone body
    ASTNodePtr clonedBody = cloneAST(funcDecl->body.get());
    if (clonedBody) {
        substituteTypes(clonedBody.get(), substitution);
    }
    
    // Substitute return type
    std::string returnType = funcDecl->returnType;
    if (!returnType.empty() && returnType[0] == '*') {
        std::string typeParamName = returnType.substr(1);
        auto it = substitution.find(typeParamName);
        if (it != substitution.end()) {
            returnType = resolver->canonicalizeTypeName(it->second);
        }
    }
    
    // Create new function declaration
    FuncDeclStmt* cloned = new FuncDeclStmt(
        funcDecl->funcName,
        returnType,
        clonedParams,
        std::move(clonedBody),
        funcDecl->line,
        funcDecl->column
    );
    
    // Copy flags but clear generic params
    cloned->isAsync = funcDecl->isAsync;
    cloned->isPublic = funcDecl->isPublic;
    cloned->isExtern = funcDecl->isExtern;
    // Do NOT copy genericParams - this is now a concrete function
    
    return cloned;
}

bool Monomorphizer::checkDepthLimit() const {
    return instantiationStack.size() < MAX_INSTANTIATION_DEPTH;
}

void Monomorphizer::addError(const std::string& message, int line, int column) {
    errors.emplace_back(message, line, column);
}

ASTNodePtr Monomorphizer::cloneAST(ASTNode* node) {
    if (!node) return nullptr;
    
    // TODO: Implement deep AST cloning
    // For now, we'll return nullptr indicating cloning not yet implemented
    // This will be implemented in the next phase
    
    return nullptr;
}

void Monomorphizer::substituteTypes(ASTNode* node,
                                   const TypeSubstitution& substitution) {
    if (!node) return;
    
    // TODO: Traverse AST and substitute *T references with concrete types
    // This will be implemented when we have proper AST traversal
}

uint64_t Monomorphizer::computeTypeHash(const TypeSubstitution& substitution) const {
    // FNV-1a hash algorithm
    uint64_t hash = 0xcbf29ce484222325ULL;
    const uint64_t prime = 0x100000001b3ULL;
    
    // Sort bindings for deterministic hashing
    std::vector<std::pair<std::string, Type*>> sortedBindings(
        substitution.begin(), substitution.end());
    std::sort(sortedBindings.begin(), sortedBindings.end());
    
    for (const auto& [paramName, type] : sortedBindings) {
        std::string typeName = resolver->canonicalizeTypeName(type);
        
        // Hash the type name
        for (char c : typeName) {
            hash ^= static_cast<uint64_t>(c);
            hash *= prime;
        }
    }
    
    return hash;
}

} // namespace sema
} // namespace aria
