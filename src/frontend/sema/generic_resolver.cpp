#include "frontend/sema/generic_resolver.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/type.h"
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
        addError("Cannot infer type arguments: function is not generic",
                callExpr ? callExpr->line : 0,
                callExpr ? callExpr->column : 0,
                "Only generic functions can have type arguments inferred");
        return substitution;
    }
    
    // Check argument count
    if (funcDecl->parameters.size() != argTypes.size()) {
        std::ostringstream msg;
        msg << "Argument count mismatch: expected " << funcDecl->parameters.size()
            << " arguments, got " << argTypes.size();
        addError(msg.str(),
                callExpr ? callExpr->line : 0, callExpr ? callExpr->column : 0,
                "Check function signature and call site");
        return substitution;
    }
    
    // Phase 1: Generate constraints from arguments
    // For each parameter, unify its type with the corresponding argument type
    for (size_t i = 0; i < funcDecl->parameters.size(); i++) {
        ParameterNode* param = static_cast<ParameterNode*>(funcDecl->parameters[i].get());
        Type* argType = argTypes[i];
        
        // Check if parameter type contains a generic parameter
        // Format: *T means the parameter is of generic type T
        std::string paramTypeName = param->typeNode ? param->typeNode->toString() : "";
        
        if (paramTypeName.empty()) continue;
        
        // Check if this is a generic type reference (*T)
        if (paramTypeName[0] == '*') {
            std::string typeParamName = paramTypeName.substr(1);
            
            // Try to unify with this type parameter
            if (!unifyTypes(nullptr, argType, substitution, typeParamName)) {
                // Error message already set by unifyTypes with conflict details
                return TypeSubstitution();
            }
        }
    }
    
    // Phase 2: Validate that all type parameters have been inferred
    for (const auto& param : funcDecl->genericParams) {
        if (substitution.find(param.name) == substitution.end()) {
            std::ostringstream msg;
            msg << "Cannot infer type parameter '" << param.name << "' from arguments";
            std::ostringstream help;
            help << "Hint: Use turbofish syntax to specify explicitly: "
                 << funcDecl->funcName << "::<" << param.name << "=type>(...)";
            addError(msg.str(),
                    callExpr ? callExpr->line : 0,
                    callExpr ? callExpr->column : 0,
                    help.str());
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
        const std::string& paramName = funcDecl->genericParams[i].name;
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
    for (const auto& param : funcDecl->genericParams) {
        auto it = substitution.find(param.name);
        if (it == substitution.end()) {
            addError("Missing type binding for parameter '" + param.name + "'");
            return false;
        }
        
        if (it->second == nullptr) {
            addError("Null type binding for parameter '" + param.name + "'");
            return false;
        }
    }
    
    return true;
}

bool GenericResolver::checkConstraints(const GenericParamInfo& param, Type* concreteType) {
    if (!concreteType) return false;
    
    // If no constraints, any type is valid
    if (!param.hasConstraints()) {
        return true;
    }
    
    // Check each trait constraint
    for (const auto& traitName : param.constraints) {
        if (!implementsTrait(concreteType, traitName)) {
            addError("Type '" + concreteType->toString() +
                    "' does not satisfy trait bound '" + traitName + "'");
            return false;
        }
    }
    
    return true;
}

bool GenericResolver::validateConstraints(
    const std::vector<GenericParamInfo>& genericParams,
    const TypeSubstitution& substitution) {
    
    bool allValid = true;
    
    for (const auto& param : genericParams) {
        auto it = substitution.find(param.name);
        if (it == substitution.end()) {
            addError("No type binding for parameter '" + param.name + "'");
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
            std::ostringstream msg;
            msg << "Type conflict: '" << paramName << "' cannot be both '"
                << boundType->toString() << "' and '" << actual->toString() << "'";
            addError(msg.str(), 0, 0,
                    "All uses of the same type parameter must have the same type");
            return false;
        }
        return true;
    }
    
    // Bind the parameter to this type
    substitution[paramName] = actual;
    return true;
}

bool GenericResolver::implementsTrait(Type* type, const std::string& traitName) {
    // Task 4 Completion: Proper trait resolution via symbol table
    if (!type) {
        return false;
    }

    // If no symbol table available, fall back to permissive behavior
    // (allows compilation to continue without trait checking in edge cases)
    if (!symbolTable) {
        return true;
    }

    // Look up the trait in the symbol table
    Symbol* traitSymbol = symbolTable->resolveSymbol(traitName);
    if (!traitSymbol || traitSymbol->kind != SymbolKind::TRAIT) {
        // Trait not found - this is an error, but report it rather than assume
        addError("Unknown trait '" + traitName + "'");
        return false;
    }

    // Get the concrete type name
    std::string typeName = type->toString();

    // Check if this type has an impl for this trait
    // The impl declarations are stored on the trait symbol
    const auto& implDecls = traitSymbol->getImplDecls();
    for (const auto* implDecl : implDecls) {
        if (implDecl && implDecl->typeName == typeName) {
            // Found a matching impl!
            return true;
        }
    }

    // No impl found for this type
    // Provide a helpful error message
    addError("Type '" + typeName + "' does not implement trait '" + traitName + "'",
             0, 0,
             "Add an impl block: impl:" + traitName + ":for:" + typeName + " = { ... }");
    return false;
}

// ============================================================================
// Monomorphizer Implementation
// ============================================================================

Monomorphizer::Monomorphizer(GenericResolver* resolver, TypeSystem* ts)
    : resolver(resolver), typeSystem(ts) {}

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
    
    // TODO(Phase 3.5+): Send specialized function to TypeChecker for recursive analysis
    // This ensures:
    //   1. Concrete types satisfy all trait constraints
    //   2. All operations in the function body are valid for the concrete types
    //   3. Any nested generic calls are also instantiated and validated
    // Once validated, set spec->analyzed = true
    // Reference: research_027_generics_templates.txt Section 3.1 "Recursive Analysis"
    
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
    ASTNodePtr returnType = funcDecl->returnType;
    if (funcDecl->returnType) {
        std::string returnTypeStr = funcDecl->returnType->toString();
        if (!returnTypeStr.empty() && returnTypeStr[0] == '*') {
            std::string typeParamName = returnTypeStr.substr(1);
            auto it = substitution.find(typeParamName);
            if (it != substitution.end()) {
                std::string newTypeName = resolver->canonicalizeTypeName(it->second);
                returnType = std::make_shared<SimpleType>(newTypeName, funcDecl->line, funcDecl->column);
            }
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
    
    // Deep clone based on node type
    switch (node->type) {
        // === Literals ===
        case ASTNode::NodeType::LITERAL: {
            LiteralExpr* lit = static_cast<LiteralExpr*>(node);
            auto cloned = std::make_unique<LiteralExpr>(*lit);
            return cloned;
        }
        
        // === Identifiers ===
        case ASTNode::NodeType::IDENTIFIER: {
            IdentifierExpr* id = static_cast<IdentifierExpr*>(node);
            return std::make_unique<IdentifierExpr>(id->name, id->line, id->column);
        }
        
        // === Binary Operations ===
        case ASTNode::NodeType::BINARY_OP: {
            BinaryExpr* bin = static_cast<BinaryExpr*>(node);
            auto left = cloneAST(bin->left.get());
            auto right = cloneAST(bin->right.get());
            return std::make_unique<BinaryExpr>(
                std::move(left), bin->op, std::move(right),
                bin->line, bin->column);
        }
        
        // === Unary Operations ===
        case ASTNode::NodeType::UNARY_OP: {
            UnaryExpr* un = static_cast<UnaryExpr*>(node);
            auto operand = cloneAST(un->operand.get());
            return std::make_unique<UnaryExpr>(
                un->op, std::move(operand), un->line, un->column);
        }
        
        // === Call Expressions ===
        case ASTNode::NodeType::CALL: {
            CallExpr* call = static_cast<CallExpr*>(node);
            auto callee = cloneAST(call->callee.get());
            std::vector<ASTNodePtr> args;
            for (const auto& arg : call->arguments) {
                args.push_back(cloneAST(arg.get()));
            }
            return std::make_unique<CallExpr>(
                std::move(callee), std::move(args), call->line, call->column);
        }
        
        // === Variable Declarations ===
        case ASTNode::NodeType::VAR_DECL: {
            VarDeclStmt* var = static_cast<VarDeclStmt*>(node);
            auto init = var->initializer ? cloneAST(var->initializer.get()) : nullptr;
            auto cloned = std::make_unique<VarDeclStmt>(
                var->typeName, var->varName, std::move(init),
                var->line, var->column);
            cloned->isWild = var->isWild;
            cloned->isConst = var->isConst;
            cloned->isStack = var->isStack;
            cloned->isGC = var->isGC;
            // Clone typeNode if it exists, otherwise create from typeName for consistency
            if (var->typeNode) {
                cloned->typeNode = cloneAST(var->typeNode.get());
            } else if (!var->typeName.empty()) {
                // Create SimpleType node from typeName to ensure typeNode is always available
                cloned->typeNode = std::make_shared<SimpleType>(var->typeName, var->line, var->column);
            }
            return cloned;
        }
        
        // === Parameters ===
        case ASTNode::NodeType::PARAMETER: {
            ParameterNode* param = static_cast<ParameterNode*>(node);
            auto defVal = param->defaultValue ? cloneAST(param->defaultValue.get()) : nullptr;
            auto clonedType = param->typeNode ? cloneAST(param->typeNode.get()) : nullptr;
            return std::make_unique<ParameterNode>(
                std::move(clonedType), param->paramName, std::move(defVal),
                param->line, param->column);
        }
        
        // === Block Statements ===
        case ASTNode::NodeType::BLOCK: {
            BlockStmt* block = static_cast<BlockStmt*>(node);
            std::vector<ASTNodePtr> stmts;
            for (const auto& stmt : block->statements) {
                stmts.push_back(cloneAST(stmt.get()));
            }
            return std::make_unique<BlockStmt>(std::move(stmts), block->line, block->column);
        }
        
        // === Return Statements ===
        case ASTNode::NodeType::RETURN: {
            ReturnStmt* ret = static_cast<ReturnStmt*>(node);
            auto val = ret->value ? cloneAST(ret->value.get()) : nullptr;
            return std::make_unique<ReturnStmt>(std::move(val), ret->line, ret->column);
        }
        
        // === If Statements ===
        case ASTNode::NodeType::IF: {
            IfStmt* ifStmt = static_cast<IfStmt*>(node);
            auto cond = cloneAST(ifStmt->condition.get());
            auto thenBranch = cloneAST(ifStmt->thenBranch.get());
            auto elseBranch = ifStmt->elseBranch ? cloneAST(ifStmt->elseBranch.get()) : nullptr;
            return std::make_unique<IfStmt>(
                std::move(cond), std::move(thenBranch), std::move(elseBranch),
                ifStmt->line, ifStmt->column);
        }
        
        // === While Statements ===
        case ASTNode::NodeType::WHILE: {
            WhileStmt* whileStmt = static_cast<WhileStmt*>(node);
            auto cond = cloneAST(whileStmt->condition.get());
            auto body = cloneAST(whileStmt->body.get());
            return std::make_unique<WhileStmt>(
                std::move(cond), std::move(body),
                whileStmt->line, whileStmt->column);
        }
        
        // === Expression Statements ===
        case ASTNode::NodeType::EXPRESSION_STMT: {
            ExpressionStmt* exprStmt = static_cast<ExpressionStmt*>(node);
            auto expr = cloneAST(exprStmt->expression.get());
            return std::make_unique<ExpressionStmt>(
                std::move(expr), exprStmt->line, exprStmt->column);
        }

        // === For Statements ===
        case ASTNode::NodeType::FOR: {
            ForStmt* forStmt = static_cast<ForStmt*>(node);
            auto init = forStmt->initializer ? cloneAST(forStmt->initializer.get()) : nullptr;
            auto cond = forStmt->condition ? cloneAST(forStmt->condition.get()) : nullptr;
            auto update = forStmt->update ? cloneAST(forStmt->update.get()) : nullptr;
            auto body = forStmt->body ? cloneAST(forStmt->body.get()) : nullptr;
            return std::make_unique<ForStmt>(
                std::move(init), std::move(cond), std::move(update), std::move(body),
                forStmt->line, forStmt->column);
        }

        // === Defer Statements ===
        case ASTNode::NodeType::DEFER: {
            DeferStmt* deferStmt = static_cast<DeferStmt*>(node);
            auto block = deferStmt->block ? cloneAST(deferStmt->block.get()) : nullptr;
            return std::make_unique<DeferStmt>(std::move(block), deferStmt->line, deferStmt->column);
        }

        // === Break/Continue Statements ===
        case ASTNode::NodeType::BREAK: {
            BreakStmt* breakStmt = static_cast<BreakStmt*>(node);
            return std::make_unique<BreakStmt>(breakStmt->label, breakStmt->line, breakStmt->column);
        }

        case ASTNode::NodeType::CONTINUE: {
            ContinueStmt* contStmt = static_cast<ContinueStmt*>(node);
            return std::make_unique<ContinueStmt>(contStmt->label, contStmt->line, contStmt->column);
        }

        // === Ternary Expressions ===
        case ASTNode::NodeType::TERNARY: {
            TernaryExpr* tern = static_cast<TernaryExpr*>(node);
            auto cond = cloneAST(tern->condition.get());
            auto trueVal = cloneAST(tern->trueValue.get());
            auto falseVal = cloneAST(tern->falseValue.get());
            return std::make_unique<TernaryExpr>(
                std::move(cond), std::move(trueVal), std::move(falseVal),
                tern->line, tern->column);
        }

        // === Index Expressions ===
        case ASTNode::NodeType::INDEX: {
            IndexExpr* idx = static_cast<IndexExpr*>(node);
            auto arr = cloneAST(idx->array.get());
            auto index = cloneAST(idx->index.get());
            return std::make_unique<IndexExpr>(std::move(arr), std::move(index), idx->line, idx->column);
        }

        // === Member Access Expressions ===
        case ASTNode::NodeType::MEMBER_ACCESS: {
            MemberAccessExpr* memAccess = static_cast<MemberAccessExpr*>(node);
            auto obj = cloneAST(memAccess->object.get());
            return std::make_unique<MemberAccessExpr>(
                std::move(obj), memAccess->member, false, memAccess->isSafeNavigation,
                memAccess->line, memAccess->column);
        }

        case ASTNode::NodeType::POINTER_MEMBER: {
            MemberAccessExpr* memAccess = static_cast<MemberAccessExpr*>(node);
            auto obj = cloneAST(memAccess->object.get());
            return std::make_unique<MemberAccessExpr>(
                std::move(obj), memAccess->member, true, memAccess->isSafeNavigation,
                memAccess->line, memAccess->column);
        }

        // === Array Literal Expressions ===
        case ASTNode::NodeType::ARRAY_LITERAL: {
            ArrayLiteralExpr* arrLit = static_cast<ArrayLiteralExpr*>(node);
            std::vector<ASTNodePtr> clonedElements;
            for (const auto& elem : arrLit->elements) {
                clonedElements.push_back(cloneAST(elem.get()));
            }
            return std::make_unique<ArrayLiteralExpr>(
                std::move(clonedElements), arrLit->line, arrLit->column);
        }

        // === Assignment Expressions ===
        case ASTNode::NodeType::ASSIGNMENT: {
            AssignmentExpr* assign = static_cast<AssignmentExpr*>(node);
            auto target = cloneAST(assign->target.get());
            auto value = cloneAST(assign->value.get());
            return std::make_unique<AssignmentExpr>(
                std::move(target), assign->op, std::move(value),
                assign->line, assign->column);
        }

        // === Move Expressions ===
        case ASTNode::NodeType::MOVE: {
            MoveExpr* moveExpr = static_cast<MoveExpr*>(node);
            auto var = cloneAST(moveExpr->variable.get());
            return std::make_unique<MoveExpr>(moveExpr->variableName, std::move(var),
                                              moveExpr->line, moveExpr->column);
        }

        // === Range Expressions ===
        case ASTNode::NodeType::RANGE: {
            RangeExpr* rangeExpr = static_cast<RangeExpr*>(node);
            auto start = rangeExpr->start ? cloneAST(rangeExpr->start.get()) : nullptr;
            auto end = rangeExpr->end ? cloneAST(rangeExpr->end.get()) : nullptr;
            return std::make_unique<RangeExpr>(
                std::move(start), std::move(end), rangeExpr->isExclusive,
                rangeExpr->line, rangeExpr->column);
        }

        // === Type Annotations ===
        case ASTNode::NodeType::TYPE_ANNOTATION: {
            SimpleType* simpleType = static_cast<SimpleType*>(node);
            return std::make_unique<SimpleType>(simpleType->typeName, simpleType->line, simpleType->column);
        }

        // === Pass/Fail Statements (Result types) ===
        case ASTNode::NodeType::PASS: {
            PassStmt* passStmt = static_cast<PassStmt*>(node);
            auto val = passStmt->value ? cloneAST(passStmt->value.get()) : nullptr;
            return std::make_unique<PassStmt>(std::move(val), passStmt->line, passStmt->column);
        }

        case ASTNode::NodeType::FAIL: {
            FailStmt* failStmt = static_cast<FailStmt*>(node);
            auto code = failStmt->errorCode ? cloneAST(failStmt->errorCode.get()) : nullptr;
            return std::make_unique<FailStmt>(std::move(code), failStmt->line, failStmt->column);
        }

        // === Await Expressions (Async) ===
        case ASTNode::NodeType::AWAIT: {
            AwaitExpr* awaitExpr = static_cast<AwaitExpr*>(node);
            auto operand = cloneAST(awaitExpr->operand.get());
            return std::make_unique<AwaitExpr>(std::move(operand), awaitExpr->line, awaitExpr->column);
        }

        // === Unwrap Expressions ===
        case ASTNode::NodeType::UNWRAP: {
            UnwrapExpr* unwrapExpr = static_cast<UnwrapExpr*>(node);
            auto result = cloneAST(unwrapExpr->result.get());
            auto defaultVal = unwrapExpr->defaultValue ? cloneAST(unwrapExpr->defaultValue.get()) : nullptr;
            return std::make_unique<UnwrapExpr>(std::move(result), std::move(defaultVal),
                                                unwrapExpr->line, unwrapExpr->column);
        }

        default:
            addError("Cannot clone AST node of type: " +
                    std::to_string(static_cast<int>(node->type)),
                    node->line, node->column);
            return nullptr;
    }
}

void Monomorphizer::substituteTypes(ASTNode* node,
                                   const TypeSubstitution& substitution) {
    if (!node) return;
    
    // Substitute types based on node type
    switch (node->type) {
        case ASTNode::NodeType::VAR_DECL: {
            VarDeclStmt* var = static_cast<VarDeclStmt*>(node);
            // Check typeName for generic type parameter (*T)
            if (!var->typeName.empty() && var->typeName[0] == '*') {
                std::string paramName = var->typeName.substr(1);
                auto it = substitution.find(paramName);
                if (it != substitution.end() && it->second) {
                    std::string newTypeName = it->second->toString();
                    var->typeName = newTypeName;
                    // Also update typeNode to keep representations in sync
                    var->typeNode = std::make_shared<SimpleType>(newTypeName, var->line, var->column);
                }
            }
            // Also check typeNode directly (may have been set by cloneAST)
            else if (var->typeNode) {
                std::string typeStr = var->typeNode->toString();
                if (!typeStr.empty() && typeStr[0] == '*') {
                    std::string paramName = typeStr.substr(1);
                    auto it = substitution.find(paramName);
                    if (it != substitution.end() && it->second) {
                        std::string newTypeName = it->second->toString();
                        var->typeName = newTypeName;
                        var->typeNode = std::make_shared<SimpleType>(newTypeName, var->line, var->column);
                    }
                }
            }
            if (var->initializer) {
                substituteTypes(var->initializer.get(), substitution);
            }
            break;
        }
        
        case ASTNode::NodeType::PARAMETER: {
            ParameterNode* param = static_cast<ParameterNode*>(node);
            if (param->typeNode) {
                std::string typeStr = param->typeNode->toString();
                if (!typeStr.empty() && typeStr[0] == '*') {
                    std::string paramName = typeStr.substr(1);
                    auto it = substitution.find(paramName);
                    if (it != substitution.end() && it->second) {
                        std::string newTypeName = it->second->toString();
                        param->typeNode = std::make_shared<SimpleType>(newTypeName, param->line, param->column);
                    }
                }
            }
            if (param->defaultValue) {
                substituteTypes(param->defaultValue.get(), substitution);
            }
            break;
        }
        
        case ASTNode::NodeType::FUNC_DECL: {
            FuncDeclStmt* func = static_cast<FuncDeclStmt*>(node);
            // Substitute return type
            if (func->returnType) {
                std::string returnTypeStr = func->returnType->toString();
                if (!returnTypeStr.empty() && returnTypeStr[0] == '*') {
                    std::string paramName = returnTypeStr.substr(1);
                    auto it = substitution.find(paramName);
                    if (it != substitution.end() && it->second) {
                        std::string newTypeName = it->second->toString();
                        func->returnType = std::make_shared<SimpleType>(newTypeName, func->line, func->column);
                    }
                }
            }
            // Substitute parameter types
            for (auto& param : func->parameters) {
                substituteTypes(param.get(), substitution);
            }
            // Substitute body
            if (func->body) {
                substituteTypes(func->body.get(), substitution);
            }
            break;
        }
        
        case ASTNode::NodeType::BINARY_OP: {
            BinaryExpr* bin = static_cast<BinaryExpr*>(node);
            substituteTypes(bin->left.get(), substitution);
            substituteTypes(bin->right.get(), substitution);
            break;
        }
        
        case ASTNode::NodeType::UNARY_OP: {
            UnaryExpr* un = static_cast<UnaryExpr*>(node);
            substituteTypes(un->operand.get(), substitution);
            break;
        }
        
        case ASTNode::NodeType::CALL: {
            CallExpr* call = static_cast<CallExpr*>(node);
            substituteTypes(call->callee.get(), substitution);
            for (auto& arg : call->arguments) {
                substituteTypes(arg.get(), substitution);
            }
            break;
        }
        
        case ASTNode::NodeType::BLOCK: {
            BlockStmt* block = static_cast<BlockStmt*>(node);
            for (auto& stmt : block->statements) {
                substituteTypes(stmt.get(), substitution);
            }
            break;
        }
        
        case ASTNode::NodeType::RETURN: {
            ReturnStmt* ret = static_cast<ReturnStmt*>(node);
            if (ret->value) {
                substituteTypes(ret->value.get(), substitution);
            }
            break;
        }
        
        case ASTNode::NodeType::IF: {
            IfStmt* ifStmt = static_cast<IfStmt*>(node);
            substituteTypes(ifStmt->condition.get(), substitution);
            substituteTypes(ifStmt->thenBranch.get(), substitution);
            if (ifStmt->elseBranch) {
                substituteTypes(ifStmt->elseBranch.get(), substitution);
            }
            break;
        }
        
        case ASTNode::NodeType::WHILE: {
            WhileStmt* whileStmt = static_cast<WhileStmt*>(node);
            substituteTypes(whileStmt->condition.get(), substitution);
            substituteTypes(whileStmt->body.get(), substitution);
            break;
        }
        
        case ASTNode::NodeType::EXPRESSION_STMT: {
            ExpressionStmt* exprStmt = static_cast<ExpressionStmt*>(node);
            substituteTypes(exprStmt->expression.get(), substitution);
            break;
        }
        
        // Other node types don't have type information to substitute
        default:
            break;
    }
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

// ============================================================================
// Struct Specialization (Session 13)
// ============================================================================

std::string Monomorphizer::requestStructSpecialization(
    StructDeclStmt* structDecl,
    const TypeSubstitution& substitution) {
    
    if (!structDecl) {
        addError("Invalid struct declaration");
        return "";
    }
    
    if (!typeSystem) {
        addError("TypeSystem not initialized for struct specialization");
        return "";
    }
    
    // Create cache key using the same format as functions
    SpecializationKey key = resolver->makeSpecializationKey(
        structDecl->structName, substitution);
    
    // Generate mangled name
    std::string mangledName = mangleName(structDecl->structName, substitution);
    
    // Check if already specialized
    Type* existing = typeSystem->getStructType(mangledName);
    if (existing) {
        return mangledName;  // Already instantiated
    }
    
    // Clone and substitute the struct
    StructDeclStmt* specialized = cloneStructAndSubstitute(structDecl, substitution);
    if (!specialized) {
        return "";
    }
    
    // Update the struct name to the mangled name
    specialized->structName = mangledName;
    
    // Register the specialized struct with TypeSystem
    // Extract field information
    std::vector<StructType::Field> fields;
    for (const auto& field : specialized->fields) {
        VarDeclStmt* fieldDecl = static_cast<VarDeclStmt*>(field.get());
        
        // Look up field type
        Type* fieldType = typeSystem->getStructType(fieldDecl->typeName);
        if (!fieldType) {
            fieldType = typeSystem->getPrimitiveType(fieldDecl->typeName);
        }
        
        if (!fieldType || fieldType->getKind() == TypeKind::ERROR) {
            addError("Unknown field type '" + fieldDecl->typeName + 
                    "' in specialized struct '" + mangledName + "'");
            return "";
        }
        
        fields.push_back(StructType::Field(fieldDecl->varName, fieldType, 0, false));
    }
    
    // Create the struct type in TypeSystem
    typeSystem->createStructType(mangledName, fields, 0, 0, false);
    
    // Clean up the cloned AST (it's no longer needed after registration)
    delete specialized;
    
    return mangledName;
}

StructDeclStmt* Monomorphizer::cloneStructAndSubstitute(
    StructDeclStmt* structDecl,
    const TypeSubstitution& substitution) {
    
    if (!structDecl) return nullptr;
    
    // Clone all fields
    std::vector<ASTNodePtr> clonedFields;
    for (const auto& field : structDecl->fields) {
        VarDeclStmt* fieldDecl = static_cast<VarDeclStmt*>(field.get());
        
        // Substitute the field type if it's a generic type reference (*T)
        std::string fieldType = fieldDecl->typeName;
        if (!fieldType.empty() && fieldType[0] == '*') {
            std::string typeParamName = fieldType.substr(1);
            auto it = substitution.find(typeParamName);
            if (it != substitution.end()) {
                fieldType = resolver->canonicalizeTypeName(it->second);
            }
        }
        
        // Create cloned field with substituted type
        auto clonedField = std::make_unique<VarDeclStmt>(
            fieldType,
            fieldDecl->varName,
            nullptr,  // Struct fields don't have initializers
            fieldDecl->line,
            fieldDecl->column
        );
        
        clonedField->isWild = fieldDecl->isWild;
        clonedField->isConst = fieldDecl->isConst;
        clonedField->isStack = fieldDecl->isStack;
        clonedField->isGC = fieldDecl->isGC;
        
        clonedFields.push_back(std::move(clonedField));
    }
    
    // Create new struct declaration
    StructDeclStmt* cloned = new StructDeclStmt(
        structDecl->structName,  // Will be updated with mangled name by caller
        clonedFields,
        structDecl->line,
        structDecl->column
    );
    
    // Do NOT copy genericParams - this is now a concrete struct
    
    return cloned;
}

} // namespace sema
} // namespace aria
