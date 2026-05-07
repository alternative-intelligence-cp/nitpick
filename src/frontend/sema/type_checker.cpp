#include "frontend/sema/type_checker.h"
#include "frontend/sema/type_checker_internal.h"
#include "frontend/sema/generic_resolver.h"
#include "frontend/sema/exhaustiveness.h"
#include "frontend/sema/definite_assignment.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/type.h"  // For ArrayType, SimpleType, etc.
#include "semantic/literal_converter.h"
#include <algorithm>
#include <sstream>
#include <variant>
#include <iostream>
#include <unordered_set>
#include <set>
#include <cmath>

namespace npk {
namespace sema {

using namespace tc_helpers;

// ============================================================================
// Phase 2: Control Flow Analysis - ResultCheckState Implementation
// ============================================================================

void ResultCheckState::markChecked(const std::string& varName) {
    states[varName] = State::CHECKED;
}

void ResultCheckState::markKnownError(const std::string& varName) {
    states[varName] = State::KNOWN_ERROR;
}

void ResultCheckState::markKnownSuccess(const std::string& varName) {
    states[varName] = State::KNOWN_SUCCESS;
}

bool ResultCheckState::isChecked(const std::string& varName) const {
    auto it = states.find(varName);
    if (it == states.end()) {
        return false;  // Never seen = unchecked
    }
    // Checked if not UNCHECKED
    return it->second != State::UNCHECKED;
}

ResultCheckState::State ResultCheckState::getState(const std::string& varName) const {
    auto it = states.find(varName);
    if (it == states.end()) {
        return State::UNCHECKED;
    }
    return it->second;
}

ResultCheckState ResultCheckState::merge(const ResultCheckState& state1, 
                                          const ResultCheckState& state2) {
    ResultCheckState result;
    
    // Collect all variables from both states
    std::set<std::string> allVars;
    for (const auto& [name, _] : state1.states) {
        allVars.insert(name);
    }
    for (const auto& [name, _] : state2.states) {
        allVars.insert(name);
    }
    
    // Merge each variable conservatively
    for (const std::string& varName : allVars) {
        State s1 = state1.getState(varName);
        State s2 = state2.getState(varName);
        
        // Conservative join logic:
        // - If both agree, use that state
        // - If one is UNCHECKED, result is UNCHECKED
        // - If both checked but different values, result is CHECKED (unknown value)
        if (s1 == s2) {
            result.states[varName] = s1;  // Both agree
        } else if (s1 == State::UNCHECKED || s2 == State::UNCHECKED) {
            result.states[varName] = State::UNCHECKED;  // One path didn't check
        } else {
            // Both checked, but different knowledge (one knows ERROR, other SUCCESS, or one CHECKED)
            result.states[varName] = State::CHECKED;  // Conservative: checked but value unknown
        }
    }
    
    return result;
}

// ============================================================================
// AST Traversal: Main Entry Point
// ============================================================================

void TypeChecker::check(ASTNode* module) {
    if (!module) {
        addError("Internal error: module AST node is null. This is a compiler bug — please report with your source code.", 0, 0);
        return;
    }

    // -------------------------------------------------------------------------
    // BUILTIN TRAIT REGISTRATION: Register fundamental traits that are always
    // available (Numeric, Sized) before any user code is processed.  Cross-
    // module trait import isn't wired up yet, so stdlib traits declared in
    // separate .aria files won't reach the main symbol table.  Registering
    // them here fixes "Unknown trait 'Numeric'" for every compilation unit.
    // Skip if the user's own code already declares a trait named "Numeric".
    // -------------------------------------------------------------------------
    {
        // Check if user code defines trait:Numeric — if so, let theirs take priority
        bool userDefinesNumeric = false;
        const std::vector<std::shared_ptr<ASTNode>>* stmtList = nullptr;
        if (module->type == ASTNode::NodeType::PROGRAM) {
            stmtList = &static_cast<ProgramNode*>(module)->declarations;
        } else if (module->type == ASTNode::NodeType::BLOCK) {
            stmtList = &static_cast<BlockStmt*>(module)->statements;
        }
        if (stmtList) {
            for (const auto& s : *stmtList) {
                if (s && s->type == ASTNode::NodeType::TRAIT_DECL) {
                    if (static_cast<TraitDeclStmt*>(s.get())->traitName == "Numeric") {
                        userDefinesNumeric = true;
                        break;
                    }
                }
            }
        }

        // Numeric — satisfied by all arithmetic primitive types
        if (!userDefinesNumeric && !symbolTable->lookupSymbol("Numeric")) {
            auto numericTrait = std::make_shared<TraitDeclStmt>(
                "Numeric", std::vector<TraitMethod>{}, 0, 0);
            syntheticNodes.push_back(numericTrait);
            checkTraitDecl(static_cast<TraitDeclStmt*>(numericTrait.get()));

            static const std::vector<std::string> numericTypes = {
                "int8", "int16", "int32", "int64",
                "uint8", "uint16", "uint32", "uint64",
                "tbb8", "tbb16", "tbb32", "tbb64",
                "frac8", "frac16", "frac32", "frac64",
                "tfp32", "tfp64", "fix256",
                "flt32", "flt64"
            };
            for (const auto& typeName : numericTypes) {
                auto implDecl = std::make_shared<ImplDeclStmt>(
                    "Numeric", typeName,
                    std::vector<std::shared_ptr<ASTNode>>{}, 0, 0);
                syntheticNodes.push_back(implDecl);
                checkImplDecl(static_cast<ImplDeclStmt*>(implDecl.get()));
            }
        }
    }

    // -------------------------------------------------------------------------
    // PRE-PASS: Forward-declare all non-generic module-level functions so that
    // mutual recursion (funcA calls funcB, funcB calls funcA) resolves cleanly.
    // Without this, calling a function declared later in the file raises
    // "Undefined identifier" during type-checking of the earlier function.
    // -------------------------------------------------------------------------
    auto preRegisterFunctions = [&](const std::vector<std::shared_ptr<ASTNode>>& stmts) {
        // STRUCT PRE-PASS: register all non-generic struct type names FIRST.
        // This prevents getPrimitiveType() from creating a fake PrimitiveType
        // with the struct name when function parameter types are resolved
        // below (the function pre-pass runs before struct declarations are
        // normally processed in the main pass).
        for (const auto& s2 : stmts) {
            if (!s2 || s2->type != ASTNode::NodeType::STRUCT_DECL) continue;
            StructDeclStmt* sd = static_cast<StructDeclStmt*>(s2.get());
            if (sd->genericParams.empty() && !typeSystem->getStructType(sd->structName)) {
                checkStructDecl(sd);  // registers StructType; main pass will silently skip
            }
        }

        // GENERIC STRUCT PRE-PASS: register generic struct templates so that
        // concrete instantiations (e.g. complex<int64>) used in non-generic
        // function signatures can be resolved during function pre-registration.
        for (const auto& s2 : stmts) {
            if (!s2 || s2->type != ASTNode::NodeType::STRUCT_DECL) continue;
            StructDeclStmt* sd = static_cast<StructDeclStmt*>(s2.get());
            if (!sd->genericParams.empty() &&
                genericStructRegistry.find(sd->structName) == genericStructRegistry.end()) {
                genericStructRegistry[sd->structName] = sd;
            }
        }

        // TYPE_DECL PRE-PASS: also pre-register Type:X = {...} struct types.
        // Type:X desugars to a struct + prefixed methods. The struct must be registered
        // before function pre-passes resolve parameter types, otherwise a fake
        // PrimitiveType("X") gets created and function arguments fail to match.
        for (const auto& s2 : stmts) {
            if (!s2 || s2->type != ASTNode::NodeType::TYPE_DECL) continue;
            TypeDeclStmt* td = static_cast<TypeDeclStmt*>(s2.get());
            if (!typeSystem->getStructType(td->typeName)) {
                // Build and register the merged struct (internal + interface fields).
                std::vector<ASTNodePtr> mergedFields;
                if (td->internalStruct) {
                    auto is = std::static_pointer_cast<StructDeclStmt>(td->internalStruct);
                    for (const auto& f : is->fields) mergedFields.push_back(f);
                }
                if (td->interfaceStruct) {
                    auto is = std::static_pointer_cast<StructDeclStmt>(td->interfaceStruct);
                    for (const auto& f : is->fields) mergedFields.push_back(f);
                }
                auto merged = std::make_shared<StructDeclStmt>(
                    td->typeName, mergedFields, td->line, td->column);
                checkStructDecl(merged.get());  // registers StructType only; methods follow in main pass
            }
        }

        for (const auto& stmt : stmts) {
            if (!stmt || stmt->type != ASTNode::NodeType::FUNC_DECL) continue;
            FuncDeclStmt* fd = static_cast<FuncDeclStmt*>(stmt.get());
            // Skip generics (handled during monomorphization)
            if (!fd->genericParams.empty()) continue;
            // Skip if already in symbol table (self-recursion pre-reg inside checkFuncDecl
            // may have registered it, but at module level nothing has run yet)
            if (symbolTable->lookupSymbol(fd->funcName)) continue;

            // Build parameter types
            std::vector<Type*> paramTypes;
            for (const auto& param : fd->parameters) {
                if (param->type == ASTNode::NodeType::PARAMETER) {
                    ParameterNode* pn = static_cast<ParameterNode*>(param.get());
                    Type* pt = resolveTypeNode(pn->typeNode.get());
                    paramTypes.push_back((pt && pt->getKind() != TypeKind::ERROR)
                        ? pt : typeSystem->getPrimitiveType("void"));
                }
            }

            // Build return type (Result<T> for body functions, raw for extern)
            Type* valueType = resolveTypeNode(fd->returnType.get());
            if (!valueType) valueType = typeSystem->getPrimitiveType("void");
            Type* retType;
            if (!fd->body) {
                retType = valueType;  // extern: raw return
            } else {
                retType = new ResultType(valueType);
            }

            Type* funcType = new FunctionType(paramTypes, retType, fd->isAsync, fd->isVariadic);
            Symbol* sym = symbolTable->defineSymbol(fd->funcName, SymbolKind::FUNCTION,
                                                     funcType, fd->line, fd->column);
            if (sym) sym->setFuncDecl(fd);
        }
    };

    // Module should be a BLOCK or PROGRAM node containing statements
    if (module->type == ASTNode::NodeType::BLOCK) {
        BlockStmt* blockStmt = static_cast<BlockStmt*>(module);
        preRegisterFunctions(blockStmt->statements);
        for (const auto& stmt : blockStmt->statements) {
            if (stmt) {
                checkStatement(stmt.get());
            }
        }
    } else if (module->type == ASTNode::NodeType::PROGRAM) {
        // If PROGRAM node exists, it likely contains a body (assume BlockStmt)
        // For now, treat as block with statements vector
        BlockStmt* blockStmt = static_cast<BlockStmt*>(module);
        preRegisterFunctions(blockStmt->statements);
        for (const auto& stmt : blockStmt->statements) {
            if (stmt) {
                checkStatement(stmt.get());
            }
        }
    } else {
        // Single statement module (edge case)
        checkStatement(module);
    }
    
    // Phase 2.3: Definite Assignment Analysis
    // Check for uninitialized variable usage after type checking
    DefiniteAssignmentAnalyzer defAssignAnalyzer;
    std::vector<AssignmentError> assignmentErrors = defAssignAnalyzer.analyze(module);
    
    // Report definite assignment errors
    for (const auto& err : assignmentErrors) {
        addError(err.message, err.line, err.column);
    }
}

// ============================================================================
// Type Inference: Main Entry Point
// ============================================================================

Type* TypeChecker::inferType(ASTNode* expr) {
    if (!expr) {
        return typeSystem->getErrorType();
    }
    
    switch (expr->type) {
        case ASTNode::NodeType::LITERAL:
            return inferLiteral(static_cast<LiteralExpr*>(expr));
        
        case ASTNode::NodeType::TEMPLATE_LITERAL:
            return inferTemplateLiteral(static_cast<TemplateLiteralExpr*>(expr));
        
        case ASTNode::NodeType::IDENTIFIER:
            return inferIdentifier(static_cast<IdentifierExpr*>(expr));
        
        case ASTNode::NodeType::BINARY_OP:
            return inferBinaryOp(static_cast<BinaryExpr*>(expr));
        
        case ASTNode::NodeType::UNARY_OP:
            return inferUnaryOp(static_cast<UnaryExpr*>(expr));
        
        case ASTNode::NodeType::CALL:
            return inferCallExpr(static_cast<CallExpr*>(expr));
        
        case ASTNode::NodeType::INDEX:
            return inferIndexExpr(static_cast<IndexExpr*>(expr));
        
        case ASTNode::NodeType::MEMBER_ACCESS:
        case ASTNode::NodeType::POINTER_MEMBER:
            return inferMemberAccessExpr(static_cast<MemberAccessExpr*>(expr));
        
        case ASTNode::NodeType::TERNARY:
            return inferTernaryExpr(static_cast<TernaryExpr*>(expr));
        
        case ASTNode::NodeType::UNWRAP:
            return inferUnwrapExpr(static_cast<UnwrapExpr*>(expr));
        
        case ASTNode::NodeType::DEFAULTS:
            return inferDefaultsExpr(static_cast<DefaultsExpr*>(expr));
        
        case ASTNode::NodeType::MOVE:
            return inferMoveExpr(static_cast<MoveExpr*>(expr));
        
        case ASTNode::NodeType::OBJECT_LITERAL:
            return inferObjectLiteral(static_cast<ObjectLiteralExpr*>(expr));
        
        case ASTNode::NodeType::ARRAY_LITERAL:
            return inferArrayLiteral(static_cast<ArrayLiteralExpr*>(expr));
        
        case ASTNode::NodeType::RANGE:
            return inferRangeExpr(static_cast<RangeExpr*>(expr));
        
        case ASTNode::NodeType::VECTOR_CONSTRUCTOR:
            return inferVectorConstructor(static_cast<VectorConstructorExpr*>(expr));
        
        case ASTNode::NodeType::AWAIT: {
            // Await expression: infer type from awaited operand
            // For async functions that return T, await returns T
            // For Future<T>, await returns T
            AwaitExpr* awaitExpr = static_cast<AwaitExpr*>(expr);
            Type* operandType = inferType(awaitExpr->operand.get());
            
            // Async functions return i8* at the IR level but their semantic return
            // type is already the inner T (set by checkFuncDecl). When Future<T> is
            // added to the trait system, unwrap it here:
            //   if (operandType->getKind() == TypeKind::FUTURE)
            //       return static_cast<FutureType*>(operandType)->getInnerType();
            return operandType;
        }
        
        case ASTNode::NodeType::CAST:
            return inferCastExpr(static_cast<CastExpr*>(expr));

        case ASTNode::NodeType::COMPTIME_EXPR:
            return inferComptimeExpr(static_cast<ComptimeExpr*>(expr));
        
        case ASTNode::NodeType::MACRO_INVOCATION:
            return inferMacroInvocation(static_cast<MacroInvocationExpr*>(expr));

        case ASTNode::NodeType::LAMBDA: {
            // Lambda expression used as a function pointer initializer.
            // The IR generator handles LAMBDA codegen directly when the declared
            // variable has a FUNCTION_TYPE typeNode.  Return a placeholder so
            // the assignment compatibility check doesn't error.
            Type* placeholder = typeSystem->getPrimitiveType("uint64");
            if (!placeholder) placeholder = typeSystem->getPrimitiveType("int64");
            return placeholder ? placeholder : typeSystem->getErrorType();
        }
        
        default:
            addError("Type inference not yet implemented for node type: " + 
                    ASTNode::nodeTypeToString(expr->type) + 
                    ". Use an explicit type annotation, e.g. Type:name = expr;", expr);
            return typeSystem->getErrorType();
    }
}

// ============================================================================
// Literal Type Inference
// ============================================================================

Type* TypeChecker::inferLiteral(LiteralExpr* expr) {
    // ========================================================================
    // PHASE 3: ZERO IMPLICIT CONVERSION - Use Explicit Type
    // ========================================================================
    // If literal has explicit type suffix (u32, i64, f32, etc.), use it DIRECTLY
    // NO inference, NO guessing - type is EXPLICIT from source code
    
    if (!expr->explicit_type.empty()) {
        // Special handling for NIL and NULL literals
        if (expr->explicit_type == "NIL") {
            // NIL literal: represents absence of value for optional types
            // Return a special marker type that can be compared with any optional
            // The type will be refined during binary operation type checking
            return typeSystem->getUnknownType();  // Will be resolved in context
        }
        
        if (expr->explicit_type == "NULL") {
            // NULL literal: represents null pointer for FFI
            // Return a special marker type that can be compared with any pointer
            // The type will be refined during binary operation type checking
            return typeSystem->getUnknownType();  // Will be resolved in context
        }
        
        if (expr->explicit_type == "UNKNOWN") {
            // unknown keyword sentinel: represents indeterminate value
            return typeSystem->getUnknownType();
        }
        
        if (expr->explicit_type == "ERR") {
            // ERR keyword sentinel: represents TBB error value
            return typeSystem->getPrimitiveType("tbb32");
        }
        
        // Convert type suffix to type system name
        std::string type_name = typeSuffixToSystemName(expr->explicit_type);
        
        // Get type from type system
        Type* explicit_type = typeSystem->getPrimitiveType(type_name);
        
        // Validate type exists
        if (!explicit_type || explicit_type == typeSystem->getErrorType()) {
            std::string errorMsg = "Unknown type '" + expr->explicit_type + "' in literal";
            std::string suggestion = findSimilarType(expr->explicit_type);
            if (!suggestion.empty()) {
                errorMsg += ". Did you mean '" + suggestion + "'?";
            }
            addError(errorMsg, expr);
            return typeSystem->getErrorType();
        }
        
        return explicit_type;
    }
    
    // ========================================================================
    // UNTYPED LITERALS: Default to int32 for integers, flt64 for floats
    // ========================================================================
    // When a literal has no explicit type suffix, it gets a default type:
    //   42        → int32 (default integer type)
    //   3.14      → flt64 (default float type)
    //   true      → bool
    //   "hello"   → string
    //
    // This allows arithmetic expressions to work naturally:
    //   tbb8:sum = 10 + 20;  // 10 and 20 default to int32, then checked if fits in tbb8
    //
    // The "fits-or-fails" validation happens during variable assignment in checkVarDecl
    
    // Check if this is a high-precision literal (has raw string)
    if (expr->hasRawValue()) {
        // High-precision literal - type will be determined by context
        // For now, assume it's for a 128+ bit type
        const std::string& raw = expr->getRawValue();
        bool is_float = (raw.find('.') != std::string::npos || 
                        raw.find('e') != std::string::npos ||
                        raw.find('E') != std::string::npos);
        
        if (is_float) {
            return typeSystem->getPrimitiveType("flt128");  // Default high-precision float
        } else {
            return typeSystem->getPrimitiveType("int128");  // Default high-precision int
        }
    }
    
    // Use std::visit to handle variant (standard literals without raw strings)
    return std::visit([this](auto&& arg) -> Type* {
        using T = std::decay_t<decltype(arg)>;
        
        if constexpr (std::is_same_v<T, int64_t>) {
            // Untyped integer literal: default to int32
            return typeSystem->getPrimitiveType("int32");
        }
        else if constexpr (std::is_same_v<T, double>) {
            // Untyped float literal: default to flt64
            return typeSystem->getPrimitiveType("flt64");
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            // Check if this is the special ERR literal
            if (arg == "ERR") {
                // ERR literal: represents TBB error sentinel
                // Type will be resolved from context (tbb8/tbb16/tbb32/tbb64)
                // For now, assume tbb32 as default
                return typeSystem->getPrimitiveType("tbb32");
            }
            // Check if this is the special unknown literal
            if (arg == "unknown") {
                // unknown literal: represents indeterminate value
                // Polymorphic like NIL - type will be resolved from context
                // Unknown gorillas = unknown frisbees = unknown (semantically)
                return typeSystem->getUnknownType();
            }
            // Regular string literal - type is always "string"
            return typeSystem->getPrimitiveType("string");
        }
        else if constexpr (std::is_same_v<T, bool>) {
            // Boolean literal
            return typeSystem->getPrimitiveType("bool");
        }
        else if constexpr (std::is_same_v<T, std::monostate>) {
            // Null literal: type will be resolved from context
            // For now, return unknown
            return typeSystem->getUnknownType();
        }
        else {
            // Should never reach here
            return typeSystem->getErrorType();
        }
    }, expr->value);
}

// ============================================================================
// Template Literal Type Inference
// ============================================================================

Type* TypeChecker::inferTemplateLiteral(TemplateLiteralExpr* expr) {
    // Template literals always result in a string type
    // But we need to type-check all interpolated expressions
    
    for (const auto& interpolation : expr->interpolations) {
        // Infer the type of each interpolated expression
        Type* interpType = inferType(interpolation.get());
        
        // Validate that the interpolated type can be converted to string.
        // Primitive types (int, uint, float, bool, string, TBB, balanced) have
        // implicit toString in the IR codegen. Struct/object types require an
        // explicit toString() method (checked when Trait system is complete).
        if (interpType->getKind() != TypeKind::PRIMITIVE &&
            interpType->getKind() != TypeKind::ERROR &&
            interpType->getKind() != TypeKind::UNKNOWN) {
            addError("Cannot interpolate value of type '" + interpType->toString() +
                    "' into template literal (only primitive types supported)", interpolation.get());
        }
    }
    
    // Template literals always produce strings
    return typeSystem->getPrimitiveType("string");
}

// ============================================================================
// Identifier Type Inference
// ============================================================================

Type* TypeChecker::inferIdentifier(IdentifierExpr* expr) {
    // Special GPU intrinsic namespace (Phase 4 - GPU/PTX Backend)
    // The `gpu` identifier is only valid in member access expressions (gpu.thread_id(), etc.)
    // Return a placeholder type that will be resolved when used in member access
    if (expr->name == "gpu") {
        // Return obj type as placeholder - actual type determined in member access
        return typeSystem->getUnknownType();
    }
    
    // Special case: $ is the implicit iteration variable in till/loop constructs
    // Its type is determined by the loop parameters.
    // loop_dollar_type_ is set by checkLoopStmt/checkTillStmt to the actual counter type.
    if (expr->name == "$") {
        if (!loop_dollar_type_.empty()) {
            return typeSystem->getPrimitiveType(loop_dollar_type_.back());
        }
        // Fallback: int64 for backwards compatibility
        return typeSystem->getPrimitiveType("int64");
    }

    // Lookup symbol in symbol table
    Symbol* symbol = symbolTable->lookupSymbol(expr->name);

    if (!symbol) {
        // Enhanced error message: check if this looks like a UFCS method call
        // Pattern: TypeName_methodName (e.g., string_lenght, array_pussh)
        size_t underscorePos = expr->name.find('_');
        if (underscorePos != std::string::npos && underscorePos > 0) {
            std::string typeName = expr->name.substr(0, underscorePos);
            std::string methodName = expr->name.substr(underscorePos + 1);

            // Check if the prefix is a known type
            if (isTypeKeyword(typeName) || typeName == "File") {
                // Try to find a similar method name
                std::string suggestion = findSimilarMethod(typeName, methodName);
                std::string availableMethods = getAvailableMethods(typeName);

                std::string errorMsg = "Type '" + typeName + "' has no method '" + methodName + "'";

                if (!suggestion.empty()) {
                    errorMsg += ". Did you mean '" + suggestion + "'?";
                }

                if (!availableMethods.empty()) {
                    errorMsg += "\n  Available methods: " + availableMethods;
                }

                addError(errorMsg, expr);
                return typeSystem->getErrorType();
            }
        }

        // Standard undefined identifier error — try to find similar names in scope
        std::string errorMsg = "Undefined identifier: '" + expr->name + "'";

        // Walk scope chain to find the closest matching symbol name
        std::string bestMatch;
        size_t bestDistance = SIZE_MAX;
        size_t maxDistance = std::max(size_t(3), expr->name.length() * 2 / 5);

        Scope* scope = symbolTable->getCurrentScope();
        while (scope) {
            for (const auto& [symName, sym] : scope->getSymbols()) {
                size_t dist = levenshteinDistance(expr->name, symName);
                if (dist < bestDistance && dist <= maxDistance && dist > 0) {
                    bestDistance = dist;
                    bestMatch = symName;
                }
            }
            scope = scope->getParent();
        }

        if (!bestMatch.empty()) {
            errorMsg += ". Did you mean '" + bestMatch + "'?";
        }

        addError(errorMsg, expr);
        return typeSystem->getErrorType();
    }

    return symbol->type;
}

// ============================================================================
// Binary Operation Type Inference
// ============================================================================

Type* TypeChecker::inferBinaryOp(BinaryExpr* expr) {
    // Infer operand types
    Type* leftType = inferType(expr->left.get());
    Type* rightType = inferType(expr->right.get());
    
    // If either operand has error, propagate error
    if (leftType->getKind() == TypeKind::ERROR || 
        rightType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // ========================================================================
    // Generic Type Constraint Handling
    // ========================================================================
    // For generic types with Numeric constraint, allow arithmetic operations
    // This enables generic numeric algorithms (complex<T: Numeric>, etc.)
    using frontend::TokenType;
    bool leftIsGeneric = (leftType->getKind() == TypeKind::GENERIC);
    bool rightIsGeneric = (rightType->getKind() == TypeKind::GENERIC);
    
    // Also check for pointer to generic (*T)
    if (leftType->getKind() == TypeKind::POINTER) {
        PointerType* ptrType = static_cast<PointerType*>(leftType);
        if (ptrType->getPointeeType()->getKind() == TypeKind::GENERIC) {
            leftIsGeneric = true;
        }
    }
    if (rightType->getKind() == TypeKind::POINTER) {
        PointerType* ptrType = static_cast<PointerType*>(rightType);
        if (ptrType->getPointeeType()->getKind() == TypeKind::GENERIC) {
            rightIsGeneric = true;
        }
    }
    
    if ((leftIsGeneric || rightIsGeneric) && 
        (expr->op.type == TokenType::TOKEN_PLUS ||
         expr->op.type == TokenType::TOKEN_MINUS ||
         expr->op.type == TokenType::TOKEN_STAR ||
         expr->op.type == TokenType::TOKEN_SLASH ||
         expr->op.type == TokenType::TOKEN_PERCENT)) {
        
        // For now, allow arithmetic on generic types
        // The generic resolver will validate Numeric constraints during instantiation
        // Return the left type as the result type (both should be the same generic param)
        return leftType;
    }
    
    // ========================================================================
    // Phase 1.5: Immutable .is_error Enforcement (for compound assignments)
    // ========================================================================
    // Block compound assignment to Result<T>.is_error (+=, -=, etc.)
    // Regular assignments go through checkAssignment()
    using frontend::TokenType;
    if (expr->op.type == TokenType::TOKEN_PLUS_EQUAL ||
        expr->op.type == TokenType::TOKEN_MINUS_EQUAL ||
        expr->op.type == TokenType::TOKEN_STAR_EQUAL ||
        expr->op.type == TokenType::TOKEN_SLASH_EQUAL ||
        expr->op.type == TokenType::TOKEN_PERCENT_EQUAL) {
        
        // Check if left side is member access to .is_error of a Result type
        if (expr->left->type == ASTNode::NodeType::MEMBER_ACCESS) {
            MemberAccessExpr* memberExpr = static_cast<MemberAccessExpr*>(expr->left.get());
            
            // Check if accessing .is_error field
            if (memberExpr->member == "is_error") {
                // Check if object is a Result type
                Type* objectType = inferType(memberExpr->object.get());
                if (objectType && objectType->getKind() == TypeKind::RESULT) {
                    // Extract variable name
                    std::string varName = getVariableName(memberExpr->object.get());
                    
                    // Check if Result variable has 'wild' modifier
                    if (!varName.empty() && wildResults.find(varName) == wildResults.end()) {
                        addError(
                            "Cannot modify .is_error on non-wild Result type (errors must propagate).\n"
                            "  Variable: " + varName + "\n"
                            "  Help: Declare as 'wild Result<T>' if you need to modify error state.\n"
                            "  Note: Modifying error state is rarely needed and should be audited.",
                            expr->line, expr->column
                        );
                        return typeSystem->getErrorType();
                    }
                }
            }
        }
    }
    
    // Context-aware literal typing for standard integers
    // If one operand is a literal and the other is a standard integer type,
    // check if the literal value fits in the target type. If so, use the target type
    // for the literal to allow comparisons and operations.
    
    // Check if right operand is an integer literal and left is a standard integer
    if (expr->right->type == ASTNode::NodeType::LITERAL &&
        isStandardIntType(leftType) && isStandardIntType(rightType)) {
        
        LiteralExpr* rightLiteral = static_cast<LiteralExpr*>(expr->right.get());
        if (std::holds_alternative<int64_t>(rightLiteral->value)) {
            int64_t value = std::get<int64_t>(rightLiteral->value);
            
            // Check if literal fits in left type - if so, use left type for the literal
            if (integerFitsInType(value, leftType->toString())) {
                rightType = leftType;  // Treat literal as having the same type as left operand
            }
        }
    }
    
    // Check if left operand is an integer literal and right is a standard integer
    if (expr->left->type == ASTNode::NodeType::LITERAL &&
        isStandardIntType(rightType) && isStandardIntType(leftType)) {
        
        LiteralExpr* leftLiteral = static_cast<LiteralExpr*>(expr->left.get());
        if (std::holds_alternative<int64_t>(leftLiteral->value)) {
            int64_t value = std::get<int64_t>(leftLiteral->value);
            
            // Check if literal fits in right type - if so, use right type for the literal
            if (integerFitsInType(value, rightType->toString())) {
                leftType = rightType;  // Treat literal as having the same type as right operand
            }
        }
    }
    
    // ========================================================================
    // Special handling for NIL/NULL comparisons
    // ========================================================================
    using frontend::TokenType;
    if (expr->op.type == TokenType::TOKEN_EQUAL_EQUAL || 
        expr->op.type == TokenType::TOKEN_BANG_EQUAL) {
        
        // Check if left is NIL literal
        bool leftIsNIL = (expr->left->type == ASTNode::NodeType::LITERAL &&
                         static_cast<LiteralExpr*>(expr->left.get())->explicit_type == "NIL");
        // Check if right is NIL literal
        bool rightIsNIL = (expr->right->type == ASTNode::NodeType::LITERAL &&
                          static_cast<LiteralExpr*>(expr->right.get())->explicit_type == "NIL");
        
        // Allow NIL == optional or optional == NIL
        if ((leftIsNIL && rightType->getKind() == TypeKind::OPTIONAL) ||
            (rightIsNIL && leftType->getKind() == TypeKind::OPTIONAL)) {
            return typeSystem->getPrimitiveType("bool");
        }
        
        // Check if left is NULL literal
        bool leftIsNULL = (expr->left->type == ASTNode::NodeType::LITERAL &&
                          static_cast<LiteralExpr*>(expr->left.get())->explicit_type == "NULL");
        // Check if right is NULL literal
        bool rightIsNULL = (expr->right->type == ASTNode::NodeType::LITERAL &&
                           static_cast<LiteralExpr*>(expr->right.get())->explicit_type == "NULL");
        
        // Allow NULL == pointer or pointer == NULL
        if ((leftIsNULL && rightType->getKind() == TypeKind::POINTER) ||
            (rightIsNULL && leftType->getKind() == TypeKind::POINTER)) {
            return typeSystem->getPrimitiveType("bool");
        }
    }
    
    // Check operator validity for given types
    return checkBinaryOperator(expr->op.type, leftType, rightType, expr);
}

// ============================================================================
// Binary Operator Type Checking
// ============================================================================

Type* TypeChecker::checkBinaryOperator(frontend::TokenType op, Type* leftType, Type* rightType,
                                        ASTNode* sourceNode) {
    using frontend::TokenType;
    
    // ========================================================================
    // Arithmetic Operators: +, -, *, /, %
    // ========================================================================
    if (op == TokenType::TOKEN_PLUS || op == TokenType::TOKEN_MINUS ||
        op == TokenType::TOKEN_STAR || op == TokenType::TOKEN_SLASH ||
        op == TokenType::TOKEN_PERCENT) {
        
        // Phase 5.3: unknown propagation - arithmetic with unknown produces unknown
        // Like NaN propagation: unknown + 5 = unknown, unknown * 2 = unknown
        // This catches unknown at compile-time type checking level
        if (leftType->getKind() == TypeKind::UNKNOWN || 
            rightType->getKind() == TypeKind::UNKNOWN) {
            // Propagate unknown type — the result is "tainted" by the unknown operand.
            // At runtime, the unknown sentinel value will propagate through arithmetic
            // just like NaN propagation in IEEE 754 floats.
            return typeSystem->getUnknownType();
        }
        
        // ====================================================================
        // String Concatenation: string + string → string
        // ====================================================================
        if (op == TokenType::TOKEN_PLUS) {
            PrimitiveType* lp = dynamic_cast<PrimitiveType*>(leftType);
            PrimitiveType* rp = dynamic_cast<PrimitiveType*>(rightType);
            if (lp && rp && lp->getName() == "string" && rp->getName() == "string") {
                return typeSystem->getPrimitiveType("string");
            }
        }
        
        // ====================================================================
        // P1-5 Phase 4: Dimensional Analysis - Arithmetic
        // ====================================================================
        bool leftIsDimensional = (leftType->getKind() == TypeKind::DIMENSIONAL);
        bool rightIsDimensional = (rightType->getKind() == TypeKind::DIMENSIONAL);
        
        // Case 1: Both operands are dimensional types
        if (leftIsDimensional && rightIsDimensional) {
            DimensionalType* leftDim = static_cast<DimensionalType*>(leftType);
            DimensionalType* rightDim = static_cast<DimensionalType*>(rightType);
            
            // Multiplication and Division: Apply dimensional algebra
            if (op == TokenType::TOKEN_STAR) {
                // J * m = J⋅m (multiply dimensions)
                Dimension resultDim = leftDim->getDimension() * rightDim->getDimension();
                Type* baseType = leftDim->getBaseType();  // Use left's base type
                return typeSystem->getDimensionalType(baseType, resultDim);
            }
            
            if (op == TokenType::TOKEN_SLASH) {
                // J / m = N (divide dimensions: kg⋅m²⋅s⁻² / m = kg⋅m⋅s⁻²)
                Dimension resultDim = leftDim->getDimension() / rightDim->getDimension();
                Type* baseType = leftDim->getBaseType();
                return typeSystem->getDimensionalType(baseType, resultDim);
            }
            
            // Addition and Subtraction: Dimensions must match!
            if (op == TokenType::TOKEN_PLUS || op == TokenType::TOKEN_MINUS) {
                if (leftDim->getDimension() != rightDim->getDimension()) {
                    std::ostringstream msg;
                    msg << "Cannot " << (op == TokenType::TOKEN_PLUS ? "add" : "subtract")
                        << " values with different dimensions: "
                        << leftDim->toString() << " and " << rightDim->toString();
                    addError(msg.str(), sourceNode);
                    return typeSystem->getErrorType();
                }
                // Same dimension, result has that dimension
                return leftType;
            }
            
            // Modulo not meaningful for dimensional types
            if (op == TokenType::TOKEN_PERCENT) {
                addError("Modulo operator not supported for dimensional types", sourceNode);
                return typeSystem->getErrorType();
            }
        }
        
        // Case 2: One dimensional, one dimensionless (scalar)
        // Allowed for * and / only
        if (leftIsDimensional || rightIsDimensional) {
            DimensionalType* dimType = leftIsDimensional ? 
                static_cast<DimensionalType*>(leftType) : 
                static_cast<DimensionalType*>(rightType);
            Type* scalarType = leftIsDimensional ? rightType : leftType;
            
            // Scalar must be numeric
            if (!isNumericType(scalarType)) {
                addError("Dimensional arithmetic requires numeric scalar, got: " + 
                        scalarType->toString(), 0, 0);
                return typeSystem->getErrorType();
            }
            
            // Multiplication and division preserve the dimensional type
            if (op == TokenType::TOKEN_STAR || op == TokenType::TOKEN_SLASH) {
                // J * 2 = J, J / 2 = J
                return dimType;
            }
            
            // Addition/subtraction not allowed with dimensionless scalars
            if (op == TokenType::TOKEN_PLUS || op == TokenType::TOKEN_MINUS) {
                std::ostringstream msg;
                msg << "Cannot " << (op == TokenType::TOKEN_PLUS ? "add" : "subtract")
                    << " dimensional type and scalar: "
                    << leftType->toString() << " and " << rightType->toString();
                addError(msg.str(), sourceNode);
                return typeSystem->getErrorType();
            }
            
            // Modulo not allowed
            if (op == TokenType::TOKEN_PERCENT) {
                addError("Modulo operator not supported for dimensional types", sourceNode);
                return typeSystem->getErrorType();
            }
        }
        
        // Vector arithmetic: component-wise operations
        // vec2 + vec2 → vec2, vec3 * vec3 → vec3, etc.
        if (leftType->getKind() == TypeKind::VECTOR && rightType->getKind() == TypeKind::VECTOR) {
            VectorType* leftVec = static_cast<VectorType*>(leftType);
            VectorType* rightVec = static_cast<VectorType*>(rightType);
            
            // Vectors must have same dimension and component type
            if (!leftType->equals(rightType)) {
                addError("Vector arithmetic requires same vector type on both sides, got '" + 
                        leftType->toString() + "' and '" + rightType->toString() + "'", 0, 0);
                return typeSystem->getErrorType();
            }
            
            // Result is the same vector type
            return leftType;
        }
        
        // Scalar * vector or vector * scalar
        if ((leftType->getKind() == TypeKind::VECTOR && rightType->getKind() == TypeKind::PRIMITIVE) ||
            (leftType->getKind() == TypeKind::PRIMITIVE && rightType->getKind() == TypeKind::VECTOR)) {
            
            VectorType* vecType = (leftType->getKind() == TypeKind::VECTOR) ? 
                                  static_cast<VectorType*>(leftType) : 
                                  static_cast<VectorType*>(rightType);
            Type* scalarType = (leftType->getKind() == TypeKind::PRIMITIVE) ? leftType : rightType;
            
            // Scalar must match vector component type
            if (!scalarType->equals(vecType->getComponentType())) {
                addError("Scalar-vector arithmetic requires scalar type to match vector component type, got scalar '" + 
                        scalarType->toString() + "' and vector component '" + vecType->getComponentType()->toString() + "'", 0, 0);
                return typeSystem->getErrorType();
            }
            
            // Result is the vector type
            return vecType;
        }
        
        // ====================================================================
        // P1-2 Phase 4: SIMD Arithmetic - Element-wise operations
        // ====================================================================
        // SIMD + SIMD → SIMD (element-wise)
        if (leftType->getKind() == TypeKind::SIMD && rightType->getKind() == TypeKind::SIMD) {
            SimdType* leftSimd = static_cast<SimdType*>(leftType);
            SimdType* rightSimd = static_cast<SimdType*>(rightType);
            
            // SIMD types must match exactly (same element type and lane count)
            if (!leftType->equals(rightType)) {
                addError("SIMD arithmetic requires matching SIMD types, got '" + 
                        leftType->toString() + "' and '" + rightType->toString() + "'", 0, 0);
                return typeSystem->getErrorType();
            }
            
            // Element type must be numeric
            Type* elemType = leftSimd->getElementType();
            if (!isNumericType(elemType)) {
                addError("SIMD arithmetic requires numeric element type, got '" + 
                        elemType->toString() + "'", 0, 0);
                return typeSystem->getErrorType();
            }
            
            // Result is the same SIMD type (element-wise operation)
            return leftType;
        }
        
        // Scalar op SIMD or SIMD op scalar (broadcast scalar to all lanes)
        if ((leftType->getKind() == TypeKind::SIMD && rightType->getKind() == TypeKind::PRIMITIVE) ||
            (leftType->getKind() == TypeKind::PRIMITIVE && rightType->getKind() == TypeKind::SIMD)) {
            
            SimdType* simdType = (leftType->getKind() == TypeKind::SIMD) ? 
                                  static_cast<SimdType*>(leftType) : 
                                  static_cast<SimdType*>(rightType);
            Type* scalarType = (leftType->getKind() == TypeKind::PRIMITIVE) ? leftType : rightType;
            
            // Scalar must match SIMD element type
            if (!scalarType->equals(simdType->getElementType())) {
                addError("Scalar-SIMD arithmetic requires scalar type to match SIMD element type, got scalar '" + 
                        scalarType->toString() + "' and SIMD element '" + simdType->getElementType()->toString() + "'", 0, 0);
                return typeSystem->getErrorType();
            }
            
            // Result is the SIMD type (scalar is broadcast to all lanes)
            return simdType;
        }
        
        //Both operands must be numeric (int*, uint*, flt*, tbb*) OR LBIM struct types
        // LBIM (Limb-Based Integral Model) structs: int1024, uint1024, int256, uint256, int512, uint512, int2048, uint2048
        PrimitiveType* leftPrim = dynamic_cast<PrimitiveType*>(leftType);
        PrimitiveType* rightPrim = dynamic_cast<PrimitiveType*>(rightType);
        StructType* leftStruct = dynamic_cast<StructType*>(leftType);
        StructType* rightStruct = dynamic_cast<StructType*>(rightType);
        
        // Helper lambda to check if a type name is an LBIM struct
        auto isLBIMType = [](const std::string& name) -> bool {
            return name == "int1024" || name == "uint1024" ||
                   name == "int256" || name == "uint256" ||
                   name == "int512" || name == "uint512" ||
                   name == "int2048" || name == "uint2048" ||
                   name == "int4096" || name == "uint4096";
        };
        
        // Check if types are valid for arithmetic
        bool leftIsLBIM = (leftStruct && isLBIMType(leftStruct->getName()));
        bool rightIsLBIM = (rightStruct && isLBIMType(rightStruct->getName()));
        bool leftIsSIMD = (leftType->getKind() == TypeKind::SIMD);
        bool rightIsSIMD = (rightType->getKind() == TypeKind::SIMD);
        
        // SIMD types should have been handled above - if we get here with SIMD, it's already returned
        // So this check is only for primitives and LBIM
        if (!leftPrim && !leftIsLBIM && !leftIsSIMD) {
            // BUG-001 fix: give an actionable hint when Result<T> is used directly in arithmetic
            if (leftType->getKind() == TypeKind::RESULT) {
                auto* resType = static_cast<ResultType*>(leftType);
                std::string inner = resType->getValueType() ? resType->getValueType()->toString() : "T";
                addError("Cannot use Result<" + inner + "> as " + inner + " in arithmetic\n"
                         "  Help: Unwrap first with '?!' (e.g., expr ?! default_val) or 'raw' keyword",
                         sourceNode);
            } else {
                addError("Arithmetic operators require numeric types (int*, uint*, flt*, tbb*), got '" + leftType->toString() + "'", sourceNode);
            }
            return typeSystem->getErrorType();
        }
        
        if (!rightPrim && !rightIsLBIM && !rightIsSIMD) {
            // BUG-001 fix: give an actionable hint when Result<T> is used directly in arithmetic
            if (rightType->getKind() == TypeKind::RESULT) {
                auto* resType = static_cast<ResultType*>(rightType);
                std::string inner = resType->getValueType() ? resType->getValueType()->toString() : "T";
                addError("Cannot use Result<" + inner + "> as " + inner + " in arithmetic\n"
                         "  Help: Unwrap first with '?!' (e.g., expr ?! default_val) or 'raw' keyword",
                         sourceNode);
            } else {
                addError("Arithmetic operators require numeric types (int*, uint*, flt*, tbb*), got '" + rightType->toString() + "'", sourceNode);
            }
            return typeSystem->getErrorType();
        }
        
        // Get type names for checking
        std::string leftName = leftPrim ? leftPrim->getName() : leftStruct->getName();
        std::string rightName = rightPrim ? rightPrim->getName() : rightStruct->getName();
        
        bool leftIsNumeric = (leftName.find("int") == 0 || leftName.find("uint") == 0 ||
                             leftName.find("flt") == 0 || leftName.find("tbb") == 0 ||
                             leftName.find("frac") == 0 || leftName.find("tfp") == 0 ||
                             leftName == "vec9" || leftName == "dvec9" ||
                             leftName == "trit" || leftName == "tryte" ||
                             leftName == "nit" || leftName == "nyte" ||
                             leftName == "fix256");  // ARIA-025: Deterministic fixed-point
        bool rightIsNumeric = (rightName.find("int") == 0 || rightName.find("uint") == 0 ||
                              rightName.find("flt") == 0 || rightName.find("tbb") == 0 ||
                              rightName.find("frac") == 0 || rightName.find("tfp") == 0 ||
                              rightName == "vec9" || rightName == "dvec9" ||
                              rightName == "trit" || rightName == "tryte" ||
                              rightName == "nit" || rightName == "nyte" ||
                              rightName == "fix256");  // ARIA-025: Deterministic fixed-point
        
        if (!leftIsNumeric || !rightIsNumeric) {
            addError("Arithmetic operators require numeric types, got '" + 
                    leftName + "' and '" + rightName + "'", sourceNode);
            return typeSystem->getErrorType();
        }
        
        // Special case: balanced type (trit/tryte/nit/nyte) arithmetic with int literals
        // Example: trit:sensor = -128; trit:result = sensor + 1;
        // The int32/int64 literal is coerced to the balanced type, result stays balanced.
        // This mirrors the existing special cases for balanced literal assignment and comparison.
        bool leftIsBalanced = (leftName == "trit" || leftName == "tryte" ||
                               leftName == "nit" || leftName == "nyte");
        bool rightIsBalanced = (rightName == "trit" || rightName == "tryte" ||
                                rightName == "nit" || rightName == "nyte");
        if (leftIsBalanced && !rightIsBalanced &&
            (rightName == "int32" || rightName == "int64")) {
            return leftType;  // Result is the balanced type
        }
        if (rightIsBalanced && !leftIsBalanced &&
            (leftName == "int32" || leftName == "int64")) {
            return rightType;  // Result is the balanced type
        }
        
        // Find common type (with promotion/widening)
        Type* resultType = findCommonType(leftType, rightType);
        if (resultType->getKind() == TypeKind::ERROR) {
            return typeSystem->getErrorType();
        }
        
        return resultType;
    }
    
    // ========================================================================
    // Bitwise Operators: &, |, ^, <<, >>
    // ========================================================================
    if (op == TokenType::TOKEN_AMPERSAND || op == TokenType::TOKEN_PIPE ||
        op == TokenType::TOKEN_CARET || op == TokenType::TOKEN_SHIFT_LEFT ||
        op == TokenType::TOKEN_SHIFT_RIGHT) {
        
        // Bitwise ops: require integer types (signed or unsigned, NOT float)
        PrimitiveType* leftPrim = dynamic_cast<PrimitiveType*>(leftType);
        PrimitiveType* rightPrim = dynamic_cast<PrimitiveType*>(rightType);
        
        if (!leftPrim || !rightPrim) {
            addError("Bitwise operators require integer types (int8-64, uint8-64), got '" + 
                    leftType->toString() + "' and '" + rightType->toString() + "'", sourceNode);
            return typeSystem->getErrorType();
        }
        
        const std::string& leftName = leftPrim->getName();
        const std::string& rightName = rightPrim->getName();
        
        // Reject float types
        if (leftName.find("flt") == 0 || rightName.find("flt") == 0) {
            addError("Bitwise operators require integer types, not floating-point. Got '" + 
                    leftName + "' and '" + rightName + "'.", sourceNode);
            return typeSystem->getErrorType();
        }
        
        // Both must be same integer type
        if (!leftType->equals(rightType)) {
            addError("Bitwise operators require same integer type on both sides. Got '" + 
                    leftName + "' and '" + rightName + "'.", sourceNode);
            return typeSystem->getErrorType();
        }
        
        return leftType;
    }
    
    // ========================================================================
    // Comparison Operators: ==, !=, <, <=, >, >=
    // ========================================================================
    if (op == TokenType::TOKEN_EQUAL_EQUAL || op == TokenType::TOKEN_BANG_EQUAL ||
        op == TokenType::TOKEN_LESS || op == TokenType::TOKEN_LESS_EQUAL ||
        op == TokenType::TOKEN_GREATER || op == TokenType::TOKEN_GREATER_EQUAL) {
        
        // Special case: Allow comparisons with unknown type (universal literal)
        // unknown can be compared with any type (three-valued logic)
        // Examples: x == unknown, unknown != 5, etc.
        if (leftType->getKind() == TypeKind::UNKNOWN || 
            rightType->getKind() == TypeKind::UNKNOWN) {
            // Comparison with unknown is always allowed
            // Result is bool (will be 'unknown' at runtime in three-valued logic, but type is bool)
            return typeSystem->getPrimitiveType("bool");
        }
        
        // ====================================================================
        // P1-2 Phase 4: SIMD Comparisons - Element-wise mask generation
        // ====================================================================
        // SIMD < SIMD → simd<bool, N> (element-wise comparison mask)
        if (leftType->getKind() == TypeKind::SIMD && rightType->getKind() == TypeKind::SIMD) {
            SimdType* leftSimd = static_cast<SimdType*>(leftType);
            SimdType* rightSimd = static_cast<SimdType*>(rightType);
            
            // SIMD types must match exactly (same element type and lane count)
            if (!leftType->equals(rightType)) {
                addError("SIMD comparison requires matching SIMD types, got '" + 
                        leftType->toString() + "' and '" + rightType->toString() + "'", 0, 0);
                return typeSystem->getErrorType();
            }
            
            // Element type must be numeric or bool
            Type* elemType = leftSimd->getElementType();
            bool validElemType = isNumericType(elemType) || 
                                (elemType->getKind() == TypeKind::PRIMITIVE && 
                                 static_cast<PrimitiveType*>(elemType)->getName() == "bool");
            if (!validElemType) {
                addError("SIMD comparison requires numeric or bool element type, got '" + 
                        elemType->toString() + "'", 0, 0);
                return typeSystem->getErrorType();
            }
            
            // Result is simd<bool, N> - a mask with same lane count
            Type* boolType = typeSystem->getPrimitiveType("bool");
            return typeSystem->getSimdType(boolType, leftSimd->getLaneCount());
        }
        
        // Scalar op SIMD or SIMD op scalar (broadcast scalar for comparison)
        if ((leftType->getKind() == TypeKind::SIMD && rightType->getKind() == TypeKind::PRIMITIVE) ||
            (leftType->getKind() == TypeKind::PRIMITIVE && rightType->getKind() == TypeKind::SIMD)) {
            
            SimdType* simdType = (leftType->getKind() == TypeKind::SIMD) ? 
                                  static_cast<SimdType*>(leftType) : 
                                  static_cast<SimdType*>(rightType);
            Type* scalarType = (leftType->getKind() == TypeKind::PRIMITIVE) ? leftType : rightType;
            
            // Scalar must match SIMD element type
            if (!scalarType->equals(simdType->getElementType())) {
                addError("Scalar-SIMD comparison requires scalar type to match SIMD element type, got scalar '" + 
                        scalarType->toString() + "' and SIMD element '" + simdType->getElementType()->toString() + "'", 0, 0);
                return typeSystem->getErrorType();
            }
            
            // Result is simd<bool, N> - a mask
            Type* boolType = typeSystem->getPrimitiveType("bool");
            return typeSystem->getSimdType(boolType, simdType->getLaneCount());
        }
        
        // Struct types do not support comparison operators — reject with a clear error
        // before the types-match check passes silently and ICmp generates invalid IR
        if (leftType->getKind() == TypeKind::STRUCT || rightType->getKind() == TypeKind::STRUCT) {
            std::string opStr = (op == TokenType::TOKEN_EQUAL_EQUAL ? "==" :
                                 op == TokenType::TOKEN_BANG_EQUAL   ? "!=" :
                                 op == TokenType::TOKEN_LESS          ? "<"  :
                                 op == TokenType::TOKEN_LESS_EQUAL    ? "<=" :
                                 op == TokenType::TOKEN_GREATER       ? ">"  : ">=");
            std::string structName = (leftType->getKind() == TypeKind::STRUCT)
                                     ? leftType->toString() : rightType->toString();
            addError("Operator '" + opStr + "' is not defined for struct type '" + structName +
                     "'. Implement an equality method on the struct instead.", sourceNode);
            return typeSystem->getErrorType();
        }

        // Special case: Allow exotic types (balanced, numeric) to be compared with integer literals
        // Example: trit:a = 1; if (a == 0) { ... }
        bool exoticIntLiteralComparison = false;
        
        // Check if left is exotic type and right is int32 (literal type)
        if ((isBalancedType(leftType) || isNumericExoticType(leftType) || isTBBType(leftType)) && 
            (rightType->toString() == "int32" || rightType->toString() == "int64")) {
            exoticIntLiteralComparison = true;
        }
        // Check if right is exotic type and left is int32 (literal type)
        else if ((isBalancedType(rightType) || isNumericExoticType(rightType) || isTBBType(rightType)) && 
                 (leftType->toString() == "int32" || leftType->toString() == "int64")) {
            exoticIntLiteralComparison = true;
        }
        
        // Require compatible types
        if (!exoticIntLiteralComparison && !leftType->equals(rightType) && !canCoerce(leftType, rightType) && !canCoerce(rightType, leftType)) {
            // BUG-001: Give a specific, actionable message when one side is Result<T>
            // and the other is its inner T — guide the user to use raw
            std::string msg;
            if (leftType->getKind() == TypeKind::RESULT) {
                auto* resType = static_cast<ResultType*>(leftType);
                if (resType->getValueType() && resType->getValueType()->equals(rightType)) {
                    msg = "Cannot compare Result<" + rightType->toString() + "> to " +
                          rightType->toString() + " directly. Use 'raw' to unwrap: raw expr op " +
                          rightType->toString();
                }
            } else if (rightType->getKind() == TypeKind::RESULT) {
                auto* resType = static_cast<ResultType*>(rightType);
                if (resType->getValueType() && resType->getValueType()->equals(leftType)) {
                    msg = "Cannot compare " + leftType->toString() + " to Result<" +
                          leftType->toString() + "> directly. Use 'raw' to unwrap: " +
                          leftType->toString() + " op raw expr";
                }
            }
            if (msg.empty()) {
                msg = "Cannot compare incompatible types: '" +
                      leftType->toString() + "' and '" + rightType->toString() + "'";
            }
            addError(msg, sourceNode);
            return typeSystem->getErrorType();
        }
        
        // Result is always bool
        return typeSystem->getPrimitiveType("bool");
    }
    
    // ========================================================================
    // Logical Operators: &&, ||
    // ========================================================================
    if (op == TokenType::TOKEN_AND_AND || op == TokenType::TOKEN_OR_OR) {
        // Strict boolean requirement (no truthiness)
        PrimitiveType* leftPrim = dynamic_cast<PrimitiveType*>(leftType);
        PrimitiveType* rightPrim = dynamic_cast<PrimitiveType*>(rightType);
        
        if (!leftPrim || leftPrim->getName() != "bool") {
            addError("Logical operator requires 'bool' type on left side, got '" + 
                    leftType->toString() + "'. Use explicit comparison (e.g., x != 0) instead of truthiness.", 0, 0);
            return typeSystem->getErrorType();
        }
        
        if (!rightPrim || rightPrim->getName() != "bool") {
            addError("Logical operator requires 'bool' type on right side, got '" + 
                    rightType->toString() + "'. Use explicit comparison (e.g., x != 0) instead of truthiness.", 0, 0);
            return typeSystem->getErrorType();
        }
        
        // Result is bool
        return typeSystem->getPrimitiveType("bool");
    }
    
    // ========================================================================
    // Null Coalescing Operator: ??
    // ========================================================================
    if (op == TokenType::TOKEN_NULL_COALESCE) {
        // Returns left side if not NIL, otherwise right side
        // Pattern: T? ?? T returns T (unwrapping the optional)
        
        // Check if left is optional
        if (leftType->getKind() == TypeKind::OPTIONAL) {
            const OptionalType* leftOptional = static_cast<const OptionalType*>(leftType);
            Type* unwrappedType = leftOptional->getWrappedType();
            
            // Right side should be compatible with the unwrapped type
            if (!rightType->equals(unwrappedType) && !rightType->isAssignableTo(unwrappedType)) {
                addError("Null coalescing operator: right side type '" + rightType->toString() + 
                        "' is not compatible with unwrapped optional type '" + unwrappedType->toString() + "'", 0, 0);
                return typeSystem->getErrorType();
            }
            
            // Result is the unwrapped type
            return unwrappedType;
        }
        
        // If left is not optional, both types should be compatible
        if (!leftType->equals(rightType) && !canCoerce(rightType, leftType)) {
            addError("Null coalescing operator requires compatible types: '" + 
                    leftType->toString() + "' and '" + rightType->toString() + "'", 0, 0);
            return typeSystem->getErrorType();
        }
        
        // Result is the left type
        return leftType;
    }
    
    // ========================================================================
    // Spaceship Operator: <=>
    // ========================================================================
    if (op == TokenType::TOKEN_SPACESHIP) {
        // Require compatible types for comparison
        if (!leftType->equals(rightType) && !canCoerce(leftType, rightType) && !canCoerce(rightType, leftType)) {
            addError("Spaceship operator requires compatible types: '" + 
                    leftType->toString() + "' and '" + rightType->toString() + "'", 0, 0);
            return typeSystem->getErrorType();
        }
        
        // Result is int (returns -1, 0, or 1)
        return typeSystem->getPrimitiveType("int32");
    }
    
    // ========================================================================
    // Assignment Operators: =, +=, -=, *=, /=, %=
    // ========================================================================
    if (op == TokenType::TOKEN_EQUAL || op == TokenType::TOKEN_PLUS_EQUAL ||
        op == TokenType::TOKEN_MINUS_EQUAL || op == TokenType::TOKEN_STAR_EQUAL ||
        op == TokenType::TOKEN_SLASH_EQUAL || op == TokenType::TOKEN_PERCENT_EQUAL) {
        
        // Check that right side is assignable to left side
        if (!rightType->isAssignableTo(leftType)) {
            addError("Cannot assign '" + rightType->toString() + 
                    "' to '" + leftType->toString() + "'", sourceNode);
            return typeSystem->getErrorType();
        }
        
        // Result type is left side type
        return leftType;
    }
    
    // Unknown operator
    addError("Unsupported binary operator. Expected arithmetic (+, -, *, /, %), "
            "bitwise (&, |, ^, <<, >>), comparison (==, !=, <, <=, >, >=), "
            "or logical (&&, ||).", sourceNode);
    return typeSystem->getErrorType();
}

// ============================================================================
// Unary Operation Type Inference
// ============================================================================

Type* TypeChecker::inferUnaryOp(UnaryExpr* expr) {
    // Infer operand type
    Type* operandType = inferType(expr->operand.get());
    
    // If operand has error, propagate error
    if (operandType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Check operator validity
    return checkUnaryOperator(expr->op.type, operandType, expr);
}

// ============================================================================
// Unary Operator Type Checking
// ============================================================================

Type* TypeChecker::checkUnaryOperator(frontend::TokenType op, Type* operandType,
                                       ASTNode* sourceNode) {
    using frontend::TokenType;
    
    PrimitiveType* primType = dynamic_cast<PrimitiveType*>(operandType);
    
    // ========================================================================
    // Arithmetic Negation: -
    // ========================================================================
    if (op == TokenType::TOKEN_MINUS) {
        // Require numeric type
        if (!primType) {
            addError("Unary minus requires numeric type, got '" + 
                    operandType->toString() + "'", sourceNode);
            return typeSystem->getErrorType();
        }
        
        const std::string& name = primType->getName();
        bool isNumeric = (name.find("int") == 0 || name.find("flt") == 0 || name.find("tbb") == 0 ||
                         name == "trit" || name == "tryte" || name == "nit" || name == "nyte" ||
                         name.find("frac") == 0 || name.find("tfp") == 0 ||
                         name == "vec9" || name == "dvec9");
        
        if (!isNumeric) {
            addError("Unary minus requires numeric type, got '" + name + "'", sourceNode);
            return typeSystem->getErrorType();
        }
        
        // Result type is same as operand
        return operandType;
    }
    
    // ========================================================================
    // Logical NOT: !
    // ========================================================================
    if (op == TokenType::TOKEN_BANG) {
        // Strict bool requirement (no truthiness)
        if (!primType || primType->getName() != "bool") {
            addError("Logical NOT requires 'bool' type, got '" + 
                    operandType->toString() + "'. Use explicit comparison (e.g., x == 0) instead of truthiness.", sourceNode);
            return typeSystem->getErrorType();
        }
        
        // Result is bool
        return typeSystem->getPrimitiveType("bool");
    }
    
    // ========================================================================
    // Bitwise NOT: ~
    // ========================================================================
    if (op == TokenType::TOKEN_TILDE) {
        // BUG-3 fix: Allow signed integer types for bitwise NOT (reject only floats)
        if (!primType || primType->getName().find("flt") == 0) {
            addError("Bitwise NOT requires integer type, got '" + 
                    operandType->toString() + "'. Cast to integer type to perform bit manipulation.", sourceNode);
            return typeSystem->getErrorType();
        }
        
        // Result type is same as operand
        return operandType;
    }
    
    // ========================================================================
    // Address-of: @
    // ========================================================================
    if (op == TokenType::TOKEN_AT) {
        // Create pointer type to the operand
        // @x where x: int32 → result type: int32@
        return typeSystem->getPointerType(operandType);
    }
    
    // ========================================================================
    // Pin: #
    // ========================================================================
    if (op == TokenType::TOKEN_HASH) {
        // Pin GC object to get wild pointer
        // #x where x: int32 → result type: int32@ (wild pointer)
        // The borrow checker will track the pinning relationship
        return typeSystem->getPointerType(operandType);
    }
    
    // ========================================================================
    // Dereference (blueprint style): value <- ptr
    // ========================================================================
    if (op == TokenType::TOKEN_LEFT_ARROW) {
        // Dereference a pointer to get the value
        // <-ptr where ptr: int32-> → result type: int32
        // The arrow shows data flow: value FROM pointer
        if (!operandType->isPointer()) {
            addError("Dereference operator (<-) requires pointer type (T@), got '" + 
                    operandType->toString() + "'. Use -> for member access through pointers.", sourceNode);
            return typeSystem->getErrorType();
        }
        // Return the pointed-to type
        return static_cast<PointerType*>(operandType)->getPointeeType();
    }
    
    // ========================================================================
    // Unwrap: ?
    // ========================================================================
    if (op == TokenType::TOKEN_QUESTION) {
        // Unwrap result type: Result<T>? → T
        // Operand must be Result<T>; returns the inner value type
        if (operandType->getKind() == TypeKind::RESULT) {
            ResultType* resultType = static_cast<ResultType*>(operandType);
            return resultType->getValueType();
        }
        addError("Unwrap operator (?) requires Result<T> type, got '" +
                operandType->toString() + "'", sourceNode);
        return typeSystem->getErrorType();
    }
    
    // ========================================================================
    // Increment/Decrement: ++ and --
    // ========================================================================
    if (op == TokenType::TOKEN_PLUS_PLUS || op == TokenType::TOKEN_MINUS_MINUS) {
        if (!primType) {
            addError("Increment/decrement requires a numeric type, got '" +
                     operandType->toString() + "'", sourceNode);
            return typeSystem->getErrorType();
        }
        const std::string& nm = primType->getName();
        bool isNumeric = (nm.find("int") == 0 || nm.find("uint") == 0 || nm.find("flt") == 0);
        if (!isNumeric) {
            addError("Increment/decrement requires an integer or float type, got '" +
                     operandType->toString() + "'", sourceNode);
            return typeSystem->getErrorType();
        }
        return operandType;  // ++/-- preserve type, return same type
    }

    // Unknown operator
    addError("Unknown unary operator. Expected -, !, ~, <-, ?, ++, or --.", sourceNode);
    return typeSystem->getErrorType();
}

// ============================================================================
// Vector Constructor Type Inference - Phase 3.3
// ============================================================================

Type* TypeChecker::inferVectorConstructor(VectorConstructorExpr* expr) {
    // Infer component types (all must match)
    if (expr->components.empty()) {
        addError("Vector constructor requires at least one component. "
                "Syntax: vec2(1.0, 2.0) or vec3(x, y, z)", expr);
        return typeSystem->getErrorType();
    }
    
    // Infer first component type
    Type* componentType = inferType(expr->components[0].get());
    if (componentType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Verify all components have the same type
    for (size_t i = 1; i < expr->components.size(); ++i) {
        Type* compType = inferType(expr->components[i].get());
        if (!compType->equals(componentType)) {
            addError("Vector components must all have the same type. "
                    "Component 1 is '" + componentType->toString() + 
                    "' but component " + std::to_string(i + 1) + " is '" + 
                    compType->toString() + "'", expr);
            return typeSystem->getErrorType();
        }
    }
    
    // Return vec2/vec3/vec9 type
    return typeSystem->getVectorType(componentType, expr->dimension);
}

// ============================================================================
// Cast Expression Type Inference (Zero Implicit Conversion)
// ============================================================================

Type* TypeChecker::inferCastExpr(CastExpr* expr) {
    // Infer the type of the expression being cast
    Type* sourceType = inferType(expr->expression.get());
    if (sourceType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Resolve target type from type annotation
    Type* targetType = resolveTypeNode(expr->targetTypeNode.get());
    if (targetType->getKind() == TypeKind::ERROR) {
        addError("Invalid target type in cast expression. "
                "Syntax: @cast(expr, TargetType) or @cast_unchecked(expr, TargetType)", expr);
        return typeSystem->getErrorType();
    }
    
    // Check if source or target are pointer types
    // Pointer types can be:
    // - PointerType instances (wild T->)
    // - Primitive types containing '*' (void*, from extern blocks)
    bool sourceIsPointer = (sourceType->getKind() == TypeKind::POINTER || 
                           sourceType->toString().find('*') != std::string::npos);
    bool targetIsPointer = (targetType->getKind() == TypeKind::POINTER ||
                           targetType->toString().find('*') != std::string::npos);
    
    // Allow pointer-to-pointer casts (e.g., void* → wild T->)
    // This is inherently unsafe (type erasure/reinterpretation)
    // Programmer takes full responsibility via explicit @cast
    if (sourceIsPointer && targetIsPointer) {
        return targetType;  // Cast succeeds, return target type
    }
    
    // Both types must be primitive numeric types for numeric casts
    if (!sourceType->isPrimitive() || !targetType->isPrimitive()) {
        addError("@cast supports numeric types and pointer types only. Cannot cast '" +
                sourceType->toString() + "' to '" + targetType->toString() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    PrimitiveType* sourcePrim = static_cast<PrimitiveType*>(sourceType);
    PrimitiveType* targetPrim = static_cast<PrimitiveType*>(targetType);
    
    // Check if both are numeric (int or float)
    bool sourceNumeric = sourcePrim->getBitWidth() > 0;
    bool targetNumeric = targetPrim->getBitWidth() > 0;
    
    if (!sourceNumeric || !targetNumeric) {
        addError("Cannot cast between non-numeric types '" + 
                 sourcePrim->getName() + "' and '" + targetPrim->getName() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    // Check for invalid casts (int <-> float requires explicit decision)
    bool sourceIsFloat = sourcePrim->isFloatingType();
    bool targetIsFloat = targetPrim->isFloatingType();
    
    if (sourceIsFloat != targetIsFloat) {
        // int <-> float cast: allowed with explicit cast
        // Runtime behavior:
        // - @cast: checked conversion (range check for float->int)
        // - @cast_unchecked: direct bitwise reinterpretation
        // This is allowed, validation handled at codegen
    }
    
    // Determine if this is a safe cast (widening)
    bool isSafe = false;
    if (sourceIsFloat == targetIsFloat) {
        // Same category (both int or both float)
        if (sourceIsFloat) {
            // Float widening: f32 -> f64 is safe
            isSafe = (targetPrim->getBitWidth() > sourcePrim->getBitWidth());
        } else {
            // Integer widening with matching signedness is safe
            // i8 -> i64 is safe, u8 -> u64 is safe
            // BUT i8 -> u64 is NOT safe (signedness mismatch)
            bool sameSign = (sourcePrim->isSignedType() == targetPrim->isSignedType());
            isSafe = sameSign && (targetPrim->getBitWidth() > sourcePrim->getBitWidth());
        }
    }
    
    // For checked casts (@cast), narrowing conversions get runtime checks
    // For unchecked casts (@cast_unchecked), no runtime checks are performed
    // Store this info for IR generation
    if (!isSafe && !expr->isUnchecked) {
        // Narrowing cast with runtime check - this is OK
        // IR generator will add overflow detection
    } else if (!isSafe && expr->isUnchecked) {
        // Unchecked narrowing cast - potentially unsafe but explicitly requested
        // IR generator will do truncation without checks
    }
    
    // Return the target type
    return targetType;
}


} // namespace sema
} // namespace npk
