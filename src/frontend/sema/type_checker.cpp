#include "frontend/sema/type_checker.h"
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

namespace aria {
namespace sema {

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
// Enhanced Error Messages - Helper Functions
// ============================================================================

// Levenshtein distance for typo detection (find similar names)
static size_t levenshteinDistance(const std::string& a, const std::string& b) {
    const size_t m = a.size();
    const size_t n = b.size();

    if (m == 0) return n;
    if (n == 0) return m;

    std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1));

    for (size_t i = 0; i <= m; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= n; ++j) dp[0][j] = j;

    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            size_t cost = (a[i-1] == b[j-1]) ? 0 : 1;
            dp[i][j] = std::min({
                dp[i-1][j] + 1,      // deletion
                dp[i][j-1] + 1,      // insertion
                dp[i-1][j-1] + cost  // substitution
            });
        }
    }

    return dp[m][n];
}

// Method registry for common types
static const std::unordered_map<std::string, std::vector<std::string>>& getTypeMethodRegistry() {
    static const std::unordered_map<std::string, std::vector<std::string>> registry = {
        // String methods
        {"string", {
            "from_char", "from_cstr", "from_int",
            "length", "is_empty", "equals",
            "contains", "starts_with", "ends_with", "substring",
            "concat", "trim", "to_upper", "to_lower",
            "to_int", "to_hex", "pad_right", "format_float"
        }},
        // Array methods
        {"array", {
            "new", "length", "push", "get", "set", "pop"
        }},
        // Map methods
        {"map", {
            "new", "length", "insert", "get", "has", "remove"
        }},
        // File methods
        {"File", {
            "open", "read_line", "read_bytes", "write", "write_bytes",
            "close", "eof", "flush", "seek", "tell",
            "read", "write_all", "delete", "exists", "size"
        }},
        // TBB type methods (tbb8, tbb16, tbb32, tbb64 share these)
        {"tbb8", {"add", "sub", "mul", "div", "mod", "neg", "abs", "is_err", "to_int"}},
        {"tbb16", {"add", "sub", "mul", "div", "mod", "neg", "abs", "is_err", "to_int"}},
        {"tbb32", {"add", "sub", "mul", "div", "mod", "neg", "abs", "is_err", "to_int"}},
        {"tbb64", {"add", "sub", "mul", "div", "mod", "neg", "abs", "is_err", "to_int"}},
    };
    return registry;
}

// Find similar method name for "Did you mean X?" suggestions
static std::string findSimilarMethod(const std::string& typeName, const std::string& methodName) {
    const auto& registry = getTypeMethodRegistry();
    auto it = registry.find(typeName);
    if (it == registry.end()) {
        return "";
    }

    const std::vector<std::string>& methods = it->second;
    std::string bestMatch;
    size_t bestDistance = SIZE_MAX;

    // Threshold: allow up to 3 character edits for short names, or 40% of length
    size_t maxDistance = std::max(size_t(3), methodName.length() * 2 / 5);

    for (const std::string& method : methods) {
        size_t dist = levenshteinDistance(methodName, method);
        if (dist < bestDistance && dist <= maxDistance) {
            bestDistance = dist;
            bestMatch = method;
        }
    }

    return bestMatch;
}

// Get formatted list of available methods for a type
static std::string getAvailableMethods(const std::string& typeName) {
    const auto& registry = getTypeMethodRegistry();
    auto it = registry.find(typeName);
    if (it == registry.end()) {
        return "";
    }

    const std::vector<std::string>& methods = it->second;
    if (methods.empty()) {
        return "";
    }

    std::string result;
    for (size_t i = 0; i < methods.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += methods[i];
    }
    return result;
}

// Find similar type name for "Did you mean?" suggestions on unknown types
static std::string findSimilarType(const std::string& typeName) {
    static const std::vector<std::string> knownTypes = {
        // Signed integers
        "int1", "int2", "int4", "int8", "int16", "int32", "int64", "int128", "int256", "int512",
        // Unsigned integers
        "uint1", "uint2", "uint4", "uint8", "uint16", "uint32", "uint64", "uint128", "uint256", "uint512",
        // TBB types
        "tbb8", "tbb16", "tbb32", "tbb64",
        // Floating point
        "flt32", "flt64", "flt128", "flt256",
        // Other primitives
        "bool", "string", "void", "NIL", "obj", "dyn", "any",
        // Collections
        "array", "map", "Result", "option",
        // Vector types
        "vec2", "vec3", "vec9", "dvec2", "dvec3", "dvec9",
        "fvec2", "fvec3", "fvec9", "ivec2", "ivec3", "ivec9"
    };

    std::string bestMatch;
    size_t bestDistance = SIZE_MAX;
    size_t maxDistance = std::max(size_t(3), typeName.length() * 2 / 5);

    // First check for case-insensitive exact match (most common mistake)
    std::string lowerInput = typeName;
    std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
    for (const std::string& known : knownTypes) {
        std::string lowerKnown = known;
        std::transform(lowerKnown.begin(), lowerKnown.end(), lowerKnown.begin(), ::tolower);
        if (lowerInput == lowerKnown && typeName != known) {
            return known;  // Exact case-insensitive match — highest confidence
        }
    }

    // Fall back to Levenshtein distance
    for (const std::string& known : knownTypes) {
        size_t dist = levenshteinDistance(typeName, known);
        if (dist < bestDistance && dist <= maxDistance) {
            bestDistance = dist;
            bestMatch = known;
        }
    }

    return bestMatch;
}

// Helper to check if a string is a type keyword (for UFCS disambiguation)
static bool isTypeKeyword(const std::string& name) {
    static const std::unordered_set<std::string> typeKeywords = {
        // Signed integers
        "int1", "int2", "int4", "int8", "int16", "int32", "int64", "int128", "int256", "int512",
        // Unsigned integers
        "uint1", "uint2", "uint4", "uint8", "uint16", "uint32", "uint64", "uint128", "uint256", "uint512",
        // TBB types
        "tbb8", "tbb16", "tbb32", "tbb64",
        // Floating point
        "flt32", "flt64", "flt128", "flt256",
        // Other primitives
        "bool", "string", "void", "NIL", "obj", "dyn", "any",
        // Collections
        "array", "map", "Result", "option"
    };
    return typeKeywords.count(name) > 0;
}

// Helper to check if a type is compatible with atomic<T>
// Atomic operations require lock-free types (integer primitives and bool)
static bool isAtomicCompatible(Type* t) {
    if (!t || t->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    std::string name = t->toString();
    
    // Allowed types: int8-64, uint8-64, bool, tbb8-64
    // Disallowed: string, floating point, composite types, pointers
    static const std::unordered_set<std::string> compatibleTypes = {
        "int8", "int16", "int32", "int64",
        "uint8", "uint16", "uint32", "uint64",
        "bool",
        "tbb8", "tbb16", "tbb32", "tbb64"
    };
    
    return compatibleTypes.count(name) > 0;
}

// ============================================================================
// AST Traversal: Main Entry Point
// ============================================================================

void TypeChecker::check(ASTNode* module) {
    if (!module) {
        addError("Null module node", 0, 0);
        return;
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

            Type* funcType = new FunctionType(paramTypes, retType, fd->isAsync, false);
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
            
            // TODO: When we have Future<T> trait system, unwrap the Future
            // For now, async functions compile to i8* but semantically return T
            // So we just return the operand's semantic type
            return operandType;
        }
        
        case ASTNode::NodeType::CAST:
            return inferCastExpr(static_cast<CastExpr*>(expr));

        case ASTNode::NodeType::COMPTIME_EXPR:
            return inferComptimeExpr(static_cast<ComptimeExpr*>(expr));

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
            addError("Type inference not implemented for node type: " + 
                    ASTNode::nodeTypeToString(expr->type), expr);
            return typeSystem->getErrorType();
    }
}

// ============================================================================
// Type Suffix to Type System Name Mapping
// ============================================================================

// Convert type suffix (u32, i64, f32, etc.) to type system name (uint32, int64, flt32, etc.)
static std::string typeSuffixToSystemName(const std::string& suffix) {
    // unsigned integers: u8 -> uint8, u16 -> uint16, etc.
    if (suffix[0] == 'u' && suffix.length() >= 2) {
        return "uint" + suffix.substr(1);
    }
    // signed integers: i8 -> int8, i16 -> int16, etc.
    if (suffix[0] == 'i' && suffix.length() >= 2) {
        return "int" + suffix.substr(1);
    }
    // floats: f32 -> flt32, f64 -> flt64, etc.
    if (suffix[0] == 'f' && suffix.length() >= 2) {
        return "flt" + suffix.substr(1);
    }
    // TBB types: tbb8, tbb16, tbb32, tbb64 (already correct)
    if (suffix.substr(0, 3) == "tbb") {
        return suffix;
    }
    // fixed-point: fix32, fix64, fix128, fix256 (already correct)
    if (suffix.substr(0, 3) == "fix") {
        return suffix;
    }
    // exotic balanced types: trit, tryte, nit, nyte (already correct)
    if (suffix == "trit" || suffix == "tryte" || suffix == "nit" || suffix == "nyte") {
        return suffix;
    }
    
    // Unknown suffix - return as-is and let type system handle error
    return suffix;
}

// ============================================================================
// Range Validation for Untyped Literals (Fits-or-Fails)
// ============================================================================

// Check if integer value fits in target type without loss
// Returns true if value fits, false otherwise
static bool integerFitsInType(int64_t value, const std::string& type_name) {
    // Signed integer types
    if (type_name == "int8")    return value >= -128 && value <= 127;
    if (type_name == "int16")   return value >= -32768 && value <= 32767;
    if (type_name == "int32")   return value >= -2147483648LL && value <= 2147483647LL;
    if (type_name == "int64")   return true;  // Always fits
    // For int128+, we can't represent in int64, so we allow it (validation happens at compile-time)
    if (type_name == "int128" || type_name == "int256" || 
        type_name == "int512" || type_name == "int1024" ||
        type_name == "int2048" || type_name == "int4096") {
        return true;  // If it fits in int64, it fits in larger types
    }
    
    // Unsigned integer types  
    if (type_name == "uint8")   return value >= 0 && value <= 255;
    if (type_name == "uint16")  return value >= 0 && value <= 65535;
    if (type_name == "uint32")  return value >= 0 && value <= 4294967295LL;
    if (type_name == "uint64")  return value >= 0;  // Always fits if non-negative
    // For uint128+, same logic as signed
    if (type_name == "uint128" || type_name == "uint256" || 
        type_name == "uint512" || type_name == "uint1024" ||
        type_name == "uint2048" || type_name == "uint4096") {
        return value >= 0;  // Must be non-negative
    }
    
    // TBB types (similar to signed)
    if (type_name == "tbb8")    return value >= -128 && value <= 127;
    if (type_name == "tbb16")   return value >= -32768 && value <= 32767;
    if (type_name == "tbb32")   return value >= -2147483648LL && value <= 2147483647LL;
    if (type_name == "tbb64")   return true;  // Always fits
    
    // Unknown type - be conservative and reject
    return false;
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
        
        // For now, we'll allow any type in interpolations
        // The IR generator will handle converting to string
        // TODO: Add implicit toString() conversion or require convertible-to-string types
        
        (void)interpType; // Suppress unused variable warning
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
    // Its type is determined by the loop parameters (usually int64)
    if (expr->name == "$") {
        // For type checking purposes, assume int64
        // The actual type will be determined by the loop parameters during IR generation
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
            // Result is unknown, but we need to type it appropriately
            // For now, return int32 as the concrete type (sentinel will be unknown at runtime)
            // TODO: In full implementation, might want to track "tainted" types
            return typeSystem->getPrimitiveType("int32");
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
            addError("Arithmetic operators require numeric types, got '" + leftType->toString() + "'", sourceNode);
            return typeSystem->getErrorType();
        }
        
        if (!rightPrim && !rightIsLBIM && !rightIsSIMD) {
            addError("Arithmetic operators require numeric types, got '" + rightType->toString() + "'", sourceNode);
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
        
        // UNSIGNED MANDATE: Only unsigned types allowed
        PrimitiveType* leftPrim = dynamic_cast<PrimitiveType*>(leftType);
        PrimitiveType* rightPrim = dynamic_cast<PrimitiveType*>(rightType);
        
        if (!leftPrim || !rightPrim) {
            addError("Bitwise operators require unsigned integer types", sourceNode);
            return typeSystem->getErrorType();
        }
        
        const std::string& leftName = leftPrim->getName();
        const std::string& rightName = rightPrim->getName();
        
        // Check for unsigned prefix
        if (leftName.find("uint") != 0 || rightName.find("uint") != 0) {
            addError("Bitwise operators require unsigned types. Got '" + 
                    leftName + "' and '" + rightName + "'. Cast to unsigned (uint*) to perform bit manipulation.", sourceNode);
            return typeSystem->getErrorType();
        }
        
        // Result type is the common type (both must be same unsigned type)
        if (!leftType->equals(rightType)) {
            addError("Bitwise operators require same unsigned type on both sides", sourceNode);
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
            // and the other is its inner T — guide the user to use raw()
            std::string msg;
            if (leftType->getKind() == TypeKind::RESULT) {
                auto* resType = static_cast<ResultType*>(leftType);
                if (resType->getValueType() && resType->getValueType()->equals(rightType)) {
                    msg = "Cannot compare Result<" + rightType->toString() + "> to " +
                          rightType->toString() + " directly. Use raw() to unwrap: raw(expr) op " +
                          rightType->toString();
                }
            } else if (rightType->getKind() == TypeKind::RESULT) {
                auto* resType = static_cast<ResultType*>(rightType);
                if (resType->getValueType() && resType->getValueType()->equals(leftType)) {
                    msg = "Cannot compare " + leftType->toString() + " to Result<" +
                          leftType->toString() + "> directly. Use raw() to unwrap: " +
                          leftType->toString() + " op raw(expr)";
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
    addError("Unknown binary operator", sourceNode);
    return typeSystem->getErrorType();
}

// ============================================================================
// Unary Operation Type Inference
// ============================================================================

Type* TypeChecker::inferUnaryOp(UnaryExpr* expr) {
    // Special case: !$x is immutable borrow syntax, not logical NOT
    if (expr->op.type == frontend::TokenType::TOKEN_BANG &&
        expr->operand && expr->operand->type == ASTNode::NodeType::UNARY_OP) {
        UnaryExpr* innerExpr = static_cast<UnaryExpr*>(expr->operand.get());
        if (innerExpr->op.type == frontend::TokenType::TOKEN_DOLLAR) {
            // This is !$x - immutable borrow, treat like $x
            return inferUnaryOp(innerExpr);
        }
    }
    
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
        // Require unsigned type
        if (!primType || primType->getName().find("uint") != 0) {
            addError("Bitwise NOT requires unsigned type, got '" + 
                    operandType->toString() + "'. Cast to unsigned (uint*) to perform bit manipulation.", sourceNode);
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
    // Borrow/Safe Reference: $
    // ========================================================================
    if (op == TokenType::TOKEN_DOLLAR) {
        // Borrow variable to create safe reference
        // $x where x: int32 → result type: int32$ (safe reference, resolves to int32@ internally)
        // The borrow checker will track the borrowing relationship
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
            addError("Dereference operator (<-) requires pointer type", sourceNode);
            return typeSystem->getErrorType();
        }
        // Return the pointed-to type
        return static_cast<PointerType*>(operandType)->getPointeeType();
    }
    
    // ========================================================================
    // Unwrap: ?
    // ========================================================================
    if (op == TokenType::TOKEN_QUESTION) {
        // Unwrap result type
        // TODO: Check that operand is result<T> type, return T
        addError("Unwrap operator (?) not yet implemented in type system", sourceNode);
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
    addError("Unknown unary operator", sourceNode);
    return typeSystem->getErrorType();
}

// ============================================================================
// Vector Constructor Type Inference - Phase 3.3
// ============================================================================

Type* TypeChecker::inferVectorConstructor(VectorConstructorExpr* expr) {
    // Infer component types (all must match)
    if (expr->components.empty()) {
        addError("Vector constructor requires components", expr);
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
            addError("All vector components must have the same type", expr);
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
        addError("Invalid target type in cast expression", expr);
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
        addError("Cast supports numeric types and pointer types only", expr);
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

// ============================================================================
// Function Call Type Inference
// ============================================================================

Type* TypeChecker::inferCallExpr(CallExpr* expr) {
    // GPU/PTX Backend Phase 4: Handle GPU intrinsic calls (gpu.thread_id(), gpu.sync_threads(), etc.)
    if (expr->callee->type == ASTNode::NodeType::MEMBER_ACCESS) {
        MemberAccessExpr* memberExpr = static_cast<MemberAccessExpr*>(expr->callee.get());
        if (memberExpr->object->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* identExpr = static_cast<IdentifierExpr*>(memberExpr->object.get());
            if (identExpr->name == "gpu") {
                // GPU intrinsics: all return int32 except sync_threads (which returns void but used as statement)
                // For simplicity, return int32 for all - sync_threads value will be ignored
                return typeSystem->getPrimitiveType("int32");
            }
        }
    }
    
    // Check for namespace-qualified static method calls: Type.method()
    // Example: string.from_char(65) -> resolves to builtin string_from_char
    // Also handles UFCS instance method calls: obj.method() -> Type_method(obj)
    // AND module function calls: module.function()
    if (expr->callee->type == ASTNode::NodeType::MEMBER_ACCESS) {
        MemberAccessExpr* memberExpr = static_cast<MemberAccessExpr*>(expr->callee.get());

        // Check if the object is an identifier (type name, module, or variable)
        if (memberExpr->object->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* identExpr = static_cast<IdentifierExpr*>(memberExpr->object.get());
            std::string name = identExpr->name;
            std::string methodName = memberExpr->member;

            // First check if this is a MODULE symbol
            Symbol* baseSymbol = symbolTable->resolveSymbol(name);
            if (baseSymbol && baseSymbol->isModule()) {
                Module* module = baseSymbol->getModuleRef();
                if (!module) {
                    addError("Module '" + name + "' has invalid module reference", expr);
                    return typeSystem->getErrorType();
                }

                // Look up function in module exports
                const auto& exports = module->getExports();
                auto it = exports.find(methodName);
                if (it == exports.end()) {
                    addError("Module '" + name + "' has no exported function '" + methodName + "'", expr);
                    return typeSystem->getErrorType();
                }

                Symbol* funcSymbol = it->second.symbol;
                if (!funcSymbol || !funcSymbol->type) {
                    addError("Invalid function symbol in module exports", expr);
                    return typeSystem->getErrorType();
                }

                // Verify it's actually a function
                if (funcSymbol->type->getKind() != TypeKind::FUNCTION) {
                    addError("'" + methodName + "' in module '" + name + "' is not a function", expr);
                    return typeSystem->getErrorType();
                }

                FunctionType* funcType = static_cast<FunctionType*>(funcSymbol->type);

                // Check argument count
                if (expr->arguments.size() != funcType->getParamTypes().size()) {
                    std::ostringstream oss;
                    oss << "Function '" + methodName + "' expects " 
                        << funcType->getParamTypes().size() << " arguments, got " 
                        << expr->arguments.size();
                    addError(oss.str(), expr);
                    return typeSystem->getErrorType();
                }

                // Type check arguments (basic type checking)
                const auto& paramTypes = funcType->getParamTypes();
                for (size_t i = 0; i < expr->arguments.size(); ++i) {
                    Type* argType = inferType(expr->arguments[i].get());
                    // Check for error type
                    if (argType->getKind() == TypeKind::ERROR) {
                        return typeSystem->getErrorType();
                    }
                    // TODO: Add proper type compatibility checking
                    // For now, just ensure no error types pass through
                }

                // Return the function's return type
                return funcType->getReturnType();
            }

            // Check if this is a type name (static method call) or a variable (UFCS)
            // Type keywords: string, int64, array, etc.
            // Also check if it's a user-defined type (struct name)
            bool isTypeName = isTypeKeyword(name) ||
                              (baseSymbol && baseSymbol->kind == SymbolKind::TYPE);

            if (isTypeName) {
                // Static method call: Type.method() -> Type_method()
                std::string builtinName = name + "_" + methodName;

                auto tempIdExpr = std::make_shared<IdentifierExpr>(builtinName, expr->line, expr->column);
                CallExpr tempCall(tempIdExpr, expr->arguments, expr->line, expr->column);
                tempCall.explicitTypeArgs = expr->explicitTypeArgs;

                return inferCallExpr(&tempCall);
            } else {
                // UFCS: obj.method() -> Type_method(obj)
                // Look up the variable to get its type
                Symbol* varSymbol = symbolTable->resolveSymbol(name);
                if (varSymbol && varSymbol->type) {
                    // Get the type name from the variable's type
                    std::string typeName = varSymbol->type->toString();

                    // Handle pointer types - strip the pointer for method lookup
                    if (!typeName.empty() && (typeName.back() == '*' || typeName.back() == '@')) {
                        typeName = typeName.substr(0, typeName.length() - 1);
                    }

                    // ====================================================================
                    // SPECIAL HANDLING: atomic<T> methods
                    // ====================================================================
                    // atomic<T> methods resolve directly without UFCS transformation
                    // Methods: load(), store(), swap(), compare_exchange(), fetch_add(), fetch_sub()
                    // Supports both "atomic<int32>" and "atomic_int32" formats
                    if (typeName.find("atomic<") == 0 || typeName.find("atomic_") == 0) {
                        // Extract inner type
                        std::string innerType;
                        if (typeName.find("atomic<") == 0) {
                            size_t start = typeName.find('<') + 1;
                            size_t end = typeName.find('>');
                            if (start != std::string::npos && end != std::string::npos) {
                                innerType = typeName.substr(start, end - start);
                            }
                        } else {
                            // atomic_int32 → int32
                            innerType = typeName.substr(7);  // Skip "atomic_"
                        }
                        
                        // Validate method name
                        std::string method = memberExpr->member;
                        if (method == "load") {
                            // load() -> T (returns inner type)
                            Type* retType = typeSystem->getPrimitiveType(innerType);
                            if (retType) return retType;
                            return typeSystem->getPrimitiveType("int32");  // fallback
                        }
                        else if (method == "store") {
                            // store(value) -> void
                            if (expr->arguments.size() != 1) {
                                addError("atomic.store() requires exactly one argument", expr);
                                return typeSystem->getErrorType();
                            }
                            Type* argType = inferType(expr->arguments[0].get());
                            if (argType->getKind() == TypeKind::ERROR) {
                                return typeSystem->getErrorType();
                            }
                            return typeSystem->getPrimitiveType("void");
                        }
                        else if (method == "swap") {
                            // swap(value) -> T (returns old value)
                            if (expr->arguments.size() != 1) {
                                addError("atomic.swap() requires exactly one argument", expr);
                                return typeSystem->getErrorType();
                            }
                            Type* argType = inferType(expr->arguments[0].get());
                            if (argType->getKind() == TypeKind::ERROR) {
                                return typeSystem->getErrorType();
                            }
                            // Return the same type as the argument
                            return argType;
                        }
                        else if (method == "fetch_add" || method == "fetch_sub") {
                            // fetch_add/sub(delta) -> T (returns old value)
                            if (expr->arguments.size() != 1) {
                                addError("atomic." + method + "() requires exactly one argument", expr);
                                return typeSystem->getErrorType();
                            }
                            Type* argType = inferType(expr->arguments[0].get());
                            if (argType->getKind() == TypeKind::ERROR) {
                                return typeSystem->getErrorType();
                            }
                            return argType;
                        }
                        else if (method == "compare_exchange") {
                            // compare_exchange(expected, desired) -> bool
                            if (expr->arguments.size() != 2) {
                                addError("atomic.compare_exchange() requires exactly two arguments", expr);
                                return typeSystem->getErrorType();
                            }
                            Type* arg1 = inferType(expr->arguments[0].get());
                            Type* arg2 = inferType(expr->arguments[1].get());
                            if (arg1->getKind() == TypeKind::ERROR || arg2->getKind() == TypeKind::ERROR) {
                                return typeSystem->getErrorType();
                            }
                            return typeSystem->getPrimitiveType("bool");
                        }
                        else {
                            addError("Unknown atomic method: " + method, expr);
                            return typeSystem->getErrorType();
                        }
                    }

                    // ====================================================================
                    // SPECIAL HANDLING: any type methods (get, set, resolve)
                    // ====================================================================
                    // any:x = ...; x.get::<int64>() -> int64
                    // any:x = ...; x.set::<int64>(42) -> void
                    // any:x = ...; x.resolve::<int64>() -> int64-> (consuming)
                    if (varSymbol->type->getKind() == TypeKind::ANY) {
                        std::string method = memberExpr->member;
                        AnyType* anyType = static_cast<AnyType*>(varSymbol->type);

                        if (method == "get") {
                            // get::<T>() — Read value as T (runtime checked)
                            if (expr->explicitTypeArgs.empty()) {
                                addError("any.get() requires a type argument: use .get::<T>()\n"
                                         "  Example: myAny.get::<int64>()", expr);
                                return typeSystem->getErrorType();
                            }
                            if (expr->arguments.size() != 0) {
                                addError("any.get::<T>() takes no arguments", expr);
                                return typeSystem->getErrorType();
                            }
                            std::string targetName = expr->explicitTypeArgs[0];
                            Type* targetType = typeSystem->getPrimitiveType(targetName);
                            if (!targetType) {
                                // Try struct lookup
                                targetType = typeSystem->getStructType(targetName);
                            }
                            if (!targetType) {
                                addError("Unknown type '" + targetName + "' in any.get::<" + targetName + ">()", expr);
                                return typeSystem->getErrorType();
                            }
                            return targetType;
                        }
                        else if (method == "set") {
                            // set::<T>(val) — Write T value (runtime checked)
                            if (expr->explicitTypeArgs.empty()) {
                                addError("any.set() requires a type argument: use .set::<T>(value)\n"
                                         "  Example: myAny.set::<int64>(42)", expr);
                                return typeSystem->getErrorType();
                            }
                            if (expr->arguments.size() != 1) {
                                addError("any.set::<T>(value) requires exactly one argument", expr);
                                return typeSystem->getErrorType();
                            }
                            std::string targetName = expr->explicitTypeArgs[0];
                            Type* targetType = typeSystem->getPrimitiveType(targetName);
                            if (!targetType) {
                                targetType = typeSystem->getStructType(targetName);
                            }
                            if (!targetType) {
                                addError("Unknown type '" + targetName + "' in any.set::<" + targetName + ">()", expr);
                                return typeSystem->getErrorType();
                            }
                            // Type check the argument against the target type
                            Type* argType = inferType(expr->arguments[0].get());
                            if (argType->getKind() == TypeKind::ERROR) {
                                return typeSystem->getErrorType();
                            }
                            if (!argType->isAssignableTo(targetType) && !canCoerce(argType, targetType)) {
                                addError("any.set::<" + targetName + ">() argument type '" + argType->toString() +
                                         "' is not compatible with '" + targetName + "'", expr);
                                return typeSystem->getErrorType();
                            }
                            return typeSystem->getPrimitiveType("void");
                        }
                        else if (method == "resolve") {
                            // resolve::<T>() — Consuming transform: any -> T-> (fat pointer)
                            if (expr->explicitTypeArgs.empty()) {
                                addError("any.resolve() requires a type argument: use .resolve::<T>()\n"
                                         "  Example: myAny.resolve::<int64>()", expr);
                                return typeSystem->getErrorType();
                            }
                            if (expr->arguments.size() != 0) {
                                addError("any.resolve::<T>() takes no arguments", expr);
                                return typeSystem->getErrorType();
                            }
                            std::string targetName = expr->explicitTypeArgs[0];
                            Type* targetType = typeSystem->getPrimitiveType(targetName);
                            if (!targetType) {
                                targetType = typeSystem->getStructType(targetName);
                            }
                            if (!targetType) {
                                addError("Unknown type '" + targetName + "' in any.resolve::<" + targetName + ">()", expr);
                                return typeSystem->getErrorType();
                            }
                            // resolve returns a fat pointer (T->) to the underlying data
                            return typeSystem->getPointerType(targetType, /*isMutable=*/false, /*isWild=*/anyType->isWildAny());
                        }
                        else {
                            addError("Unknown method '" + method + "' on any type.\n"
                                     "  Available methods: .get::<T>(), .set::<T>(value), .resolve::<T>()", expr);
                            return typeSystem->getErrorType();
                        }
                    }

                    // ====================================================================
                    // SPECIAL HANDLING: dyn Trait method calls (dynamic dispatch)
                    // ====================================================================
                    // dyn Trait:x = ...; x.method(args) -> dispatched via vtable
                    // Type checker validates method exists on trait and returns its type
                    if (typeName.substr(0, 4) == "dyn ") {
                        std::string traitName = typeName.substr(4);
                        std::string method = memberExpr->member;

                        // Look up the trait symbol
                        Symbol* traitSym = symbolTable->lookupSymbol(traitName);
                        if (!traitSym || !traitSym->isTrait()) {
                            addError("Unknown trait '" + traitName + "' in dyn dispatch", expr);
                            return typeSystem->getErrorType();
                        }

                        TraitDeclStmt* traitDecl = traitSym->getTraitDecl();
                        if (!traitDecl) {
                            addError("Trait '" + traitName + "' has no declaration", expr);
                            return typeSystem->getErrorType();
                        }

                        // Find the method in the trait's method list
                        for (const auto& tm : traitDecl->methods) {
                            if (tm.name == method) {
                                // Resolve the return type
                                Type* retType = typeSystem->getPrimitiveType(tm.returnType);
                                if (!retType) {
                                    retType = typeSystem->getStructType(tm.returnType);
                                }
                                if (!retType) {
                                    // Trait methods return Result<T> at ABI level,
                                    // but the declared return type is the inner type
                                    retType = typeSystem->getPrimitiveType("int32");  // fallback
                                }
                                return retType;
                            }
                        }

                        addError("Trait '" + traitName + "' has no method '" + method + "'", expr);
                        return typeSystem->getErrorType();
                    }

                    // Handle generic types - use base type for method lookup
                    // e.g., array<string> -> array
                    size_t anglePos = typeName.find('<');
                    if (anglePos != std::string::npos) {
                        typeName = typeName.substr(0, anglePos);
                    }

                    // Construct the UFCS function name: typename_methodname
                    std::string ufcsName = typeName + "_" + methodName;

                    // Create the UFCS call with the object as the first argument
                    std::vector<ASTNodePtr> newArgs;
                    newArgs.push_back(memberExpr->object);  // Inject object as first arg
                    for (auto& arg : expr->arguments) {
                        newArgs.push_back(arg);  // Append original arguments
                    }

                    auto tempIdExpr = std::make_shared<IdentifierExpr>(ufcsName, expr->line, expr->column);
                    CallExpr tempCall(tempIdExpr, newArgs, expr->line, expr->column);
                    tempCall.explicitTypeArgs = expr->explicitTypeArgs;

                    return inferCallExpr(&tempCall);
                }
            }
        }
    }
    
    // Check for builtin functions first
    if (expr->callee->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* idExpr = static_cast<IdentifierExpr*>(expr->callee.get());
        
        // ====================================================================
        // COMPILER INTRINSICS - Compile-Time Reflection
        // ====================================================================
        
        // @sizeof(type) -> int64 - Returns size of type in bytes
        if (idExpr->name == "@sizeof" || idExpr->name == "sizeof") {
            if (expr->arguments.size() != 1) {
                addError("@sizeof() requires exactly one argument (type or expression)", expr);
                return typeSystem->getErrorType();
            }
            // For now, just return int64 - actual value computed at compile time
            return typeSystem->getPrimitiveType("int64");
        }
        
        // @alignof(type) -> int64 - Returns alignment requirement in bytes
        if (idExpr->name == "@alignof" || idExpr->name == "alignof") {
            if (expr->arguments.size() != 1) {
                addError("@alignof() requires exactly one argument (type)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // @typeof(expr) -> type - Returns type of expression
        if (idExpr->name == "@typeof" || idExpr->name == "typeof") {
            if (expr->arguments.size() != 1) {
                addError("@typeof() requires exactly one argument (expression)", expr);
                return typeSystem->getErrorType();
            }
            // Returns the type itself (for metaprogramming)
            Type* argType = inferType(expr->arguments[0].get());
            return argType;
        }
        
        // @offsetof(type, field) -> int64 - Returns byte offset of field in struct
        if (idExpr->name == "@offsetof" || idExpr->name == "offsetof") {
            if (expr->arguments.size() != 2) {
                addError("@offsetof() requires exactly two arguments (type, field_name)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // ====================================================================
        // COMPILER HINTS - Branch Prediction & Performance
        // ====================================================================
        
        // @likely(bool) -> bool - Hints that condition is usually true
        if (idExpr->name == "@likely" || idExpr->name == "likely") {
            if (expr->arguments.size() != 1) {
                addError("@likely() requires exactly one argument (bool condition)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // @unlikely(bool) -> bool - Hints that condition is usually false
        if (idExpr->name == "@unlikely" || idExpr->name == "unlikely") {
            if (expr->arguments.size() != 1) {
                addError("@unlikely() requires exactly one argument (bool condition)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // @prefetch(ptr) -> void - Prefetch data into cache
        if (idExpr->name == "@prefetch" || idExpr->name == "prefetch") {
            if (expr->arguments.size() != 1) {
                addError("@prefetch() requires exactly one argument (pointer)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }
        
        // ====================================================================
        // DEBUG & ASSERTIONS
        // ====================================================================
        
        // @assert(condition) -> void - Runtime assertion
        if (idExpr->name == "@assert" || idExpr->name == "assert") {
            if (expr->arguments.size() < 1 || expr->arguments.size() > 2) {
                addError("@assert() requires 1-2 arguments (condition, optional message)", expr);
                return typeSystem->getErrorType();
            }
            Type* condType = inferType(expr->arguments[0].get());
            if (condType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }
        
        // @unreachable() -> never - Marks code path as impossible
        if (idExpr->name == "@unreachable" || idExpr->name == "unreachable") {
            if (expr->arguments.size() != 0) {
                addError("@unreachable() takes no arguments", expr);
                return typeSystem->getErrorType();
            }
            // Return 'never' type (for now using error type as placeholder)
            return typeSystem->getErrorType();
        }
        
        // ====================================================================
        // SIMD HORIZONTAL REDUCTIONS (P1-2 Phase 5)
        // ====================================================================
        // Reduce SIMD vector to scalar value
        
        // @simd_sum(simd<T, N>) -> T - Sum all vector elements
        if (idExpr->name == "@simd_sum" || idExpr->name == "simd_sum") {
            if (expr->arguments.size() != 1) {
                addError("@simd_sum() requires exactly one SIMD vector argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->getKind() != TypeKind::SIMD) {
                addError("@simd_sum() requires SIMD vector argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            // Return element type: simd<T, N> -> T
            SimdType* simdType = static_cast<SimdType*>(argType);
            return simdType->getElementType();
        }
        
        // @simd_product(simd<T, N>) -> T - Multiply all vector elements
        if (idExpr->name == "@simd_product" || idExpr->name == "simd_product") {
            if (expr->arguments.size() != 1) {
                addError("@simd_product() requires exactly one SIMD vector argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->getKind() != TypeKind::SIMD) {
                addError("@simd_product() requires SIMD vector argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            SimdType* simdType = static_cast<SimdType*>(argType);
            return simdType->getElementType();
        }
        
        // @simd_min(simd<T, N>) -> T - Find minimum element
        if (idExpr->name == "@simd_min" || idExpr->name == "simd_min") {
            if (expr->arguments.size() != 1) {
                addError("@simd_min() requires exactly one SIMD vector argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->getKind() != TypeKind::SIMD) {
                addError("@simd_min() requires SIMD vector argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            SimdType* simdType = static_cast<SimdType*>(argType);
            return simdType->getElementType();
        }
        
        // @simd_max(simd<T, N>) -> T - Find maximum element
        if (idExpr->name == "@simd_max" || idExpr->name == "simd_max") {
            if (expr->arguments.size() != 1) {
                addError("@simd_max() requires exactly one SIMD vector argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->getKind() != TypeKind::SIMD) {
                addError("@simd_max() requires SIMD vector argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            SimdType* simdType = static_cast<SimdType*>(argType);
            return simdType->getElementType();
        }
        
        // ====================================================================
        // SIMD BOOLEAN OPERATIONS (P1-2 Phase 6)
        // ====================================================================
        // Boolean reductions for SIMD mask operations
        
        // @simd_any(simd<bool, N>) -> bool - True if ANY lane is true
        if (idExpr->name == "@simd_any" || idExpr->name == "simd_any") {
            if (expr->arguments.size() != 1) {
                addError("simd_any() requires exactly one SIMD boolean vector argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->getKind() != TypeKind::SIMD) {
                addError("simd_any() requires SIMD vector argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            // Validate element type is bool
            SimdType* simdType = static_cast<SimdType*>(argType);
            Type* elemType = simdType->getElementType();
            if (elemType->toString() != "bool") {
                addError("simd_any() requires simd<bool, N> argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // @simd_all(simd<bool, N>) -> bool - True if ALL lanes are true
        if (idExpr->name == "@simd_all" || idExpr->name == "simd_all") {
            if (expr->arguments.size() != 1) {
                addError("simd_all() requires exactly one SIMD boolean vector argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->getKind() != TypeKind::SIMD) {
                addError("simd_all() requires SIMD vector argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            // Validate element type is bool
            SimdType* simdType = static_cast<SimdType*>(argType);
            Type* elemType = simdType->getElementType();
            if (elemType->toString() != "bool") {
                addError("simd_all() requires simd<bool, N> argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // @simd_select(simd<bool, N>, simd<T, N>, simd<T, N>) -> simd<T, N>
        // SIMD conditional select: for each lane, pick from true_vals or false_vals based on mask
        // Equivalent to: mask ? true_vals : false_vals (per-lane)
        if (idExpr->name == "@simd_select" || idExpr->name == "simd_select") {
            if (expr->arguments.size() != 3) {
                addError("simd_select() requires exactly three arguments (mask, true_vals, false_vals)", expr);
                return typeSystem->getErrorType();
            }
            
            Type* maskType = inferType(expr->arguments[0].get());
            Type* trueType = inferType(expr->arguments[1].get());
            Type* falseType = inferType(expr->arguments[2].get());
            
            if (maskType->getKind() == TypeKind::ERROR || 
                trueType->getKind() == TypeKind::ERROR ||
                falseType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Validate mask is simd<bool, N>
            if (maskType->getKind() != TypeKind::SIMD) {
                addError("simd_select() first argument must be SIMD boolean mask, got '" + maskType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            SimdType* maskSimd = static_cast<SimdType*>(maskType);
            if (maskSimd->getElementType()->toString() != "bool") {
                addError("simd_select() mask must be simd<bool, N>, got '" + maskType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            // Validate true_vals and false_vals are SIMD with same type and lane count
            if (trueType->getKind() != TypeKind::SIMD || falseType->getKind() != TypeKind::SIMD) {
                addError("simd_select() second and third arguments must be SIMD vectors", expr);
                return typeSystem->getErrorType();
            }
            
            SimdType* trueSimd = static_cast<SimdType*>(trueType);
            SimdType* falseSimd = static_cast<SimdType*>(falseType);
            
            // Check lane counts match
            if (maskSimd->getLaneCount() != trueSimd->getLaneCount() ||
                trueSimd->getLaneCount() != falseSimd->getLaneCount()) {
                addError("simd_select() all vectors must have same lane count", expr);
                return typeSystem->getErrorType();
            }
            
            // Check true and false element types match
            if (!trueSimd->getElementType()->equals(falseSimd->getElementType())) {
                addError("simd_select() true and false vectors must have same element type", expr);
                return typeSystem->getErrorType();
            }
            
            // Return the SIMD type of true/false values
            return trueType;
        }
        
        // @simd_broadcast(T:value, int32:lanes) -> simd<T, lanes>
        // Create a SIMD vector with all lanes set to the same scalar value
        if (idExpr->name == "@simd_broadcast" || idExpr->name == "simd_broadcast") {
            if (expr->arguments.size() != 2) {
                addError("simd_broadcast() requires exactly two arguments (value, lane_count)", expr);
                return typeSystem->getErrorType();
            }
            
            Type* scalarType = inferType(expr->arguments[0].get());
            Type* laneCountType = inferType(expr->arguments[1].get());
            
            if (scalarType->getKind() == TypeKind::ERROR || laneCountType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Validate scalar value is a primitive numeric type
            if (!scalarType->isPrimitive() || !isNumericType(scalarType)) {
                addError("simd_broadcast() first argument must be a numeric scalar, got '" + scalarType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            // Validate lane count is an integer
            if (!laneCountType->isPrimitive()) {
                addError("simd_broadcast() second argument must be an integer lane count", expr);
                return typeSystem->getErrorType();
            }
            
            PrimitiveType* laneCountPrim = static_cast<PrimitiveType*>(laneCountType);
            const std::string& lcName = laneCountPrim->getName();
            if (lcName.find("int") != 0 && lcName.find("uint") != 0) {
                addError("simd_broadcast() lane count must be an integer type, got '" + lcName + "'", expr);
                return typeSystem->getErrorType();
            }
            
            // Extract lane count value (must be a compile-time constant)
            // For now, we'll accept any integer expression and validate at runtime
            // In the future, we could require a literal and extract its value
            
            // For type checking, we need to determine the lane count
            // Check if it's an integer literal
            size_t laneCount = 0;
            if (expr->arguments[1]->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* litNode = static_cast<LiteralExpr*>(expr->arguments[1].get());
                if (std::holds_alternative<int64_t>(litNode->value)) {
                    laneCount = std::get<int64_t>(litNode->value);
                } else {
                    addError("simd_broadcast() lane count must be an integer literal", expr);
                    return typeSystem->getErrorType();
                }
            } else {
                addError("simd_broadcast() lane count must be a compile-time integer literal", expr);
                return typeSystem->getErrorType();
            }
            
            // Validate lane count is reasonable
            if (laneCount == 0 || laneCount > 64) {
                addError("simd_broadcast() lane count must be between 1 and 64, got: " + std::to_string(laneCount), expr);
                return typeSystem->getErrorType();
            }
            
            // Create and return SIMD type
            return typeSystem->getSimdType(scalarType, laneCount);
        }
        
        // @simd_load(T*:ptr, int32:lanes) -> simd<T, lanes>
        // Load a SIMD vector from memory (aligned load)
        if (idExpr->name == "@simd_load" || idExpr->name == "simd_load") {
            if (expr->arguments.size() != 2) {
                addError("simd_load() requires exactly two arguments (pointer, lane_count)", expr);
                return typeSystem->getErrorType();
            }
            
            Type* ptrType = inferType(expr->arguments[0].get());
            Type* laneCountType = inferType(expr->arguments[1].get());
            
            if (ptrType->getKind() == TypeKind::ERROR || laneCountType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Validate first argument is a pointer
            if (ptrType->getKind() != TypeKind::POINTER) {
                addError("simd_load() first argument must be a pointer, got '" + ptrType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            PointerType* ptr = static_cast<PointerType*>(ptrType);
            Type* elementType = ptr->getPointeeType();
            
            // Validate element type is numeric
            if (!elementType->isPrimitive() || !isNumericType(elementType)) {
                addError("simd_load() pointer must point to numeric type, got '" + elementType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            // Extract lane count (same validation as simd_broadcast)
            size_t laneCount = 0;
            if (expr->arguments[1]->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* litNode = static_cast<LiteralExpr*>(expr->arguments[1].get());
                if (std::holds_alternative<int64_t>(litNode->value)) {
                    laneCount = std::get<int64_t>(litNode->value);
                } else {
                    addError("simd_load() lane count must be an integer literal", expr);
                    return typeSystem->getErrorType();
                }
            } else {
                addError("simd_load() lane count must be a compile-time integer literal", expr);
                return typeSystem->getErrorType();
            }
            
            // Validate lane count
            if (laneCount == 0 || laneCount > 64) {
                addError("simd_load() lane count must be between 1 and 64, got: " + std::to_string(laneCount), expr);
                return typeSystem->getErrorType();
            }
            
            // Return simd<T, lanes> where T is the pointed-to type
            return typeSystem->getSimdType(elementType, laneCount);
        }
        
        // @simd_store(T*:ptr, simd<T, N>:vec) -> void
        // Store a SIMD vector to memory (aligned store)
        if (idExpr->name == "@simd_store" || idExpr->name == "simd_store") {
            if (expr->arguments.size() != 2) {
                addError("simd_store() requires exactly two arguments (pointer, vector)", expr);
                return typeSystem->getErrorType();
            }
            
            Type* ptrType = inferType(expr->arguments[0].get());
            Type* vecType = inferType(expr->arguments[1].get());
            
            if (ptrType->getKind() == TypeKind::ERROR || vecType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Validate first argument is a pointer
            if (ptrType->getKind() != TypeKind::POINTER) {
                addError("simd_store() first argument must be a pointer, got '" + ptrType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            // Validate second argument is a SIMD vector
            if (vecType->getKind() != TypeKind::SIMD) {
                addError("simd_store() second argument must be a SIMD vector, got '" + vecType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            PointerType* ptr = static_cast<PointerType*>(ptrType);
            SimdType* vec = static_cast<SimdType*>(vecType);
            
            Type* ptrElementType = ptr->getPointeeType();
            Type* vecElementType = vec->getElementType();
            
            // Validate pointer element type matches SIMD element type
            if (!ptrElementType->equals(vecElementType)) {
                addError("simd_store() pointer element type must match SIMD element type, got pointer to '" + 
                        ptrElementType->toString() + "' and SIMD of '" + vecElementType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            // Return void
            return typeSystem->getPrimitiveType("void");
        }
        
        // @breakpoint() -> void - Insert debugger breakpoint
        if (idExpr->name == "@breakpoint" || idExpr->name == "breakpoint") {
            if (expr->arguments.size() != 0) {
                addError("@breakpoint() takes no arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }
        
        // @trap() -> never - Trigger intentional trap/abort
        if (idExpr->name == "@trap" || idExpr->name == "trap") {
            if (expr->arguments.size() != 0) {
                addError("@trap() takes no arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }
        
        // ====================================================================
        // BIT MANIPULATION INTRINSICS
        // ====================================================================
        
        // @bswap16/32/64(value) -> same - Byte swap
        if (idExpr->name == "@bswap16" || idExpr->name == "bswap16") {
            if (expr->arguments.size() != 1) {
                addError("@bswap16() requires exactly one argument (int16)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int16");
        }
        if (idExpr->name == "@bswap32" || idExpr->name == "bswap32") {
            if (expr->arguments.size() != 1) {
                addError("@bswap32() requires exactly one argument (int32)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        if (idExpr->name == "@bswap64" || idExpr->name == "bswap64") {
            if (expr->arguments.size() != 1) {
                addError("@bswap64() requires exactly one argument (int64)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // @clz32/64(value) -> int32 - Count leading zeros
        if (idExpr->name == "@clz32" || idExpr->name == "clz32") {
            if (expr->arguments.size() != 1) {
                addError("@clz32() requires exactly one argument (int32)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        if (idExpr->name == "@clz64" || idExpr->name == "clz64") {
            if (expr->arguments.size() != 1) {
                addError("@clz64() requires exactly one argument (int64)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        // @ctz32/64(value) -> int32 - Count trailing zeros
        if (idExpr->name == "@ctz32" || idExpr->name == "ctz32") {
            if (expr->arguments.size() != 1) {
                addError("@ctz32() requires exactly one argument (int32)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        if (idExpr->name == "@ctz64" || idExpr->name == "ctz64") {
            if (expr->arguments.size() != 1) {
                addError("@ctz64() requires exactly one argument (int64)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        // @popcount32/64(value) -> int32 - Count set bits
        if (idExpr->name == "@popcount32" || idExpr->name == "popcount32") {
            if (expr->arguments.size() != 1) {
                addError("@popcount32() requires exactly one argument (int32)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        if (idExpr->name == "@pop count64" || idExpr->name == "popcount64") {
            if (expr->arguments.size() != 1) {
                addError("@popcount64() requires exactly one argument (int64)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        // ====================================================================
        // FRAC (EXACT RATIONAL) ARITHMETIC INTRINSICS
        // ====================================================================
        // Mixed-number fractions: frac32 = {tbb32 whole, tbb32 num, tbb32 denom}
        // Exact rational arithmetic with automatic GCD reduction
        
        // frac*_from_parts(whole, num, denom) -> frac*
        // Construct fraction from components (validates denom != 0)
        for (const char* width : {"8", "16", "32", "64"}) {
            std::string funcName = "frac" + std::string(width) + "_from_parts";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 3) {
                    addError(funcName + "() requires exactly 3 arguments (whole, numerator, denominator)", expr);
                    return typeSystem->getErrorType();
                }
                // Validate all arguments are integers (will be converted to tbb internally)
                for (size_t i = 0; i < 3; i++) {
                    Type* argType = inferType(expr->arguments[i].get());
                    if (!argType->isPrimitive() || !isNumericType(argType)) {
                        addError(funcName + "() arguments must be integer values", expr);
                        return typeSystem->getErrorType();
                    }
                }
                return typeSystem->getPrimitiveType("frac" + std::string(width));
            }
        }
        
        // frac*_add/sub/mul/div(a, b) -> frac*
        // Arithmetic operations with automatic GCD reduction
        for (const char* width : {"8", "16", "32", "64"}) {
            std::string frac_type = "frac" + std::string(width);
            for (const char* op : {"add", "sub", "mul", "div"}) {
                std::string funcName = frac_type + "_" + op;
                if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                    if (expr->arguments.size() != 2) {
                        addError(funcName + "() requires exactly 2 arguments", expr);
                        return typeSystem->getErrorType();
                    }
                    // Validate both arguments are the same frac type
                    Type* arg0Type = inferType(expr->arguments[0].get());
                    Type* arg1Type = inferType(expr->arguments[1].get());
                    
                    if (arg0Type->toString() != frac_type || arg1Type->toString() != frac_type) {
                        addError(funcName + "() requires two " + frac_type + " arguments", expr);
                        return typeSystem->getErrorType();
                    }
                    return typeSystem->getPrimitiveType(frac_type);
                }
            }
        }
        
        // frac*_neg(a) -> frac*
        // Negation
        for (const char* width : {"8", "16", "32", "64"}) {
            std::string frac_type = "frac" + std::string(width);
            std::string funcName = frac_type + "_neg";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 1) {
                    addError(funcName + "() requires exactly 1 argument", expr);
                    return typeSystem->getErrorType();
                }
                Type* argType = inferType(expr->arguments[0].get());
                if (argType->toString() != frac_type) {
                    addError(funcName + "() requires a " + frac_type + " argument", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType(frac_type);
            }
        }
        
        // frac*_cmp(a, b) -> int32
        // Comparison: returns -1 (a < b), 0 (a == b), +1 (a > b)
        for (const char* width : {"8", "16", "32", "64"}) {
            std::string frac_type = "frac" + std::string(width);
            std::string funcName = frac_type + "_cmp";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 2) {
                    addError(funcName + "() requires exactly 2 arguments", expr);
                    return typeSystem->getErrorType();
                }
                Type* arg0Type = inferType(expr->arguments[0].get());
                Type* arg1Type = inferType(expr->arguments[1].get());
                
                if (arg0Type->toString() != frac_type || arg1Type->toString() != frac_type) {
                    addError(funcName + "() requires two " + frac_type + " arguments", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType("int32");
            }
        }
        
        // frac*_to_int(a) -> int32/int64
        // Convert fraction to integer (truncates towards zero)
        for (const char* width : {"8", "16", "32", "64"}) {
            std::string frac_type = "frac" + std::string(width);
            std::string funcName = frac_type + "_to_int";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 1) {
                    addError(funcName + "() requires exactly 1 argument", expr);
                    return typeSystem->getErrorType();
                }
                Type* argType = inferType(expr->arguments[0].get());
                if (argType->toString() != frac_type) {
                    addError(funcName + "() requires a " + frac_type + " argument", expr);
                    return typeSystem->getErrorType();
                }
                // Return int32 for frac8/16/32, int64 for frac64
                if (std::string(width) == "64") {
                    return typeSystem->getPrimitiveType("int64");
                } else {
                    return typeSystem->getPrimitiveType("int32");
                }
            }
        }
        
        // frac*_to_float(a) -> float32/float64
        // Convert fraction to floating point
        for (const char* width : {"8", "16", "32", "64"}) {
            std::string frac_type = "frac" + std::string(width);
            std::string funcName = frac_type + "_to_float";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 1) {
                    addError(funcName + "() requires exactly 1 argument", expr);
                    return typeSystem->getErrorType();
                }
                Type* argType = inferType(expr->arguments[0].get());
                if (argType->toString() != frac_type) {
                    addError(funcName + "() requires a " + frac_type + " argument", expr);
                    return typeSystem->getErrorType();
                }
                // Return flt32 for frac8/16/32, flt64 for frac64
                if (std::string(width) == "64") {
                    return typeSystem->getPrimitiveType("flt64");
                } else {
                    return typeSystem->getPrimitiveType("flt32");
                }
            }
        }
        
        // ====================================================================
        // TFP (TWISTED FLOATING POINT) INTRINSICS
        // ====================================================================
        // Deterministic cross-platform floating-point with TBB safety model
        // tfp32: {tbb16 exp, tbb16 mant}
        // tfp64: {tbb16 exp, tbb48 mant}
        
        // tfp*_from_parts(exp, mant) -> tfp*
        for (const char* width : {"32", "64"}) {
            std::string funcName = "tfp" + std::string(width) + "_from_parts";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 2) {
                    addError(funcName + "() requires exactly 2 arguments (exponent, mantissa)", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType("tfp" + std::string(width));
            }
        }
        
        // tfp*_from_double(double) -> tfp*
        for (const char* width : {"32", "64"}) {
            std::string funcName = "tfp" + std::string(width) + "_from_double";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 1) {
                    addError(funcName + "() requires exactly 1 argument (double value)", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType("tfp" + std::string(width));
            }
        }
        
        // tfp*_to_double(tfp*) -> flt64
        for (const char* width : {"32", "64"}) {
            std::string funcName = "tfp" + std::string(width) + "_to_double";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 1) {
                    addError(funcName + "() requires exactly 1 argument (tfp value)", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType("flt64");
            }
        }
        
        // tfp* arithmetic: add, sub, mul, div
        for (const char* width : {"32", "64"}) {
            for (const char* op : {"add", "sub", "mul", "div"}) {
                std::string funcName = "tfp" + std::string(width) + "_" + op;
                if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                    if (expr->arguments.size() != 2) {
                        addError(funcName + "() requires exactly 2 arguments", expr);
                        return typeSystem->getErrorType();
                    }
                    return typeSystem->getPrimitiveType("tfp" + std::string(width));
                }
            }
        }
        
        // tfp*_neg(tfp*) -> tfp*
        for (const char* width : {"32", "64"}) {
            std::string funcName = "tfp" + std::string(width) + "_neg";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 1) {
                    addError(funcName + "() requires exactly 1 argument", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType("tfp" + std::string(width));
            }
        }
        
        // tfp*_cmp(tfp*, tfp*) -> int32
        for (const char* width : {"32", "64"}) {
            std::string funcName = "tfp" + std::string(width) + "_cmp";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 2) {
                    addError(funcName + "() requires exactly 2 arguments", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType("int32");
            }           
        }
        
        // tfp* math: sqrt, sin, cos, exp, log (single argument)
        for (const char* width : {"32", "64"}) {
            for (const char* mathfunc : {"sqrt", "sin", "cos", "exp", "log"}) {
                std::string funcName = "tfp" + std::string(width) + "_" + mathfunc;
                if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                    if (expr->arguments.size() != 1) {
                        addError(funcName + "() requires exactly 1 argument", expr);
                        return typeSystem->getErrorType();
                    }
                    return typeSystem->getPrimitiveType("tfp" + std::string(width));
                }
            }
        }
        
        // tfp*_pow(base, exp) -> tfp*
        for (const char* width : {"32", "64"}) {
            std::string funcName = "tfp" + std::string(width) + "_pow";
            if (idExpr->name == funcName || idExpr->name == "@" + funcName) {
                if (expr->arguments.size() != 2) {
                    addError(funcName + "() requires exactly 2 arguments (base, exponent)", expr);
                    return typeSystem->getErrorType();
                }
                return typeSystem->getPrimitiveType("tfp" + std::string(width));
            }
        }
        
        // ====================================================================
        // UNKNOWN STATE MANAGEMENT (Phase 5.2)
        // ====================================================================
        
        // ok(value) -> value - Strips "unknown taint", acknowledges unknown state
        // Purpose: Force explicit acknowledgment when propagating unknown values
        // Example: int32:y = ok(x);  // "I know this might be unknown, that's OK"
        if (idExpr->name == "ok") {
            if (expr->arguments.size() != 1) {
                addError("ok() requires exactly one argument (value to acknowledge)", expr);
                return typeSystem->getErrorType();
            }
            // Infer type of argument
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // ok() is a pass-through - returns same type as argument
            // The "taint stripping" is handled in assignment checking
            return argType;
        }
        
        // ====================================================================
        // STANDARD I/O BUILTINS
        // ====================================================================
        
        // Builtin: print() - write string as-is (no newline)
        // Builtin: println() - write string + newline
        if (idExpr->name == "print" || idExpr->name == "println") {
            if (expr->arguments.size() != 1) {
                addError(idExpr->name + "() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            // Infer argument type to validate it
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // print/println return i64 (bytes written from write() syscall)
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Builtin: stdout_write(string) -> int64
        if (idExpr->name == "stdout_write") {
            if (expr->arguments.size() != 1) {
                addError("stdout_write() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Should be string type
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Builtin: stderr_write(string) -> int64
        if (idExpr->name == "stderr_write") {
            if (expr->arguments.size() != 1) {
                addError("stderr_write() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Builtin: stddbg_write(string) -> int64
        if (idExpr->name == "stddbg_write") {
            if (expr->arguments.size() != 1) {
                addError("stddbg_write() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Builtin: stdin_read_line() -> string
        if (idExpr->name == "stdin_read_line") {
            if (expr->arguments.size() != 0) {
                addError("stdin_read_line() takes no arguments", expr);
                return typeSystem->getErrorType();
            }
            // Returns string (char*)
            return typeSystem->getPrimitiveType("string");
        }

        // Builtin: stdin_read_all() -> string
        if (idExpr->name == "stdin_read_all") {
            if (expr->arguments.size() != 0) {
                addError("stdin_read_all() takes no arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }
        
        // ====================================================================
        // SYSCALL BUILTINS — sys() / sys!!() / sys!!!()
        // Tiered system call interface (v0.4.0)
        // ====================================================================
        
        if (idExpr->name == "sys" || idExpr->name == "sys!!" || idExpr->name == "sys!!!") {
            bool isSafe = (idExpr->name == "sys");
            bool isRaw  = (idExpr->name == "sys!!!");
            
            // Need at least 1 argument (the syscall number)
            if (expr->arguments.size() < 1) {
                addError(idExpr->name + "() requires at least one argument (syscall constant)", expr);
                return typeSystem->getErrorType();
            }
            
            // Maximum 7 arguments (syscall number + 6 args)
            if (expr->arguments.size() > 7) {
                addError(idExpr->name + "() accepts at most 7 arguments (syscall number + 6 args), got " +
                         std::to_string(expr->arguments.size()), expr);
                return typeSystem->getErrorType();
            }
            
            // --- Typed return categories for sys() ---
            // Most syscalls return int64 (count, fd, 0/-errno).
            // STRING_BUF syscalls write into a user buffer and return length;
            // the compiler auto-allocates the buffer and converts to Aria string.
            enum class SysReturnCategory { INT64, STRING_BUF };
            static const std::unordered_map<std::string, SysReturnCategory> syscallReturnTypes = {
                {"GETCWD",   SysReturnCategory::STRING_BUF},
                {"READLINK", SysReturnCategory::STRING_BUF},
            };
            
            // Typed-return syscalls: validate arg count (v0.4.1)
            // STRING_BUF syscalls manage their own buffer — user provides fewer args.
            {
                IdentifierExpr* firstId = dynamic_cast<IdentifierExpr*>(expr->arguments[0].get());
                if (firstId) {
                    auto retIt = syscallReturnTypes.find(firstId->name);
                    if (retIt != syscallReturnTypes.end() && retIt->second == SysReturnCategory::STRING_BUF) {
                        if (firstId->name == "GETCWD" && expr->arguments.size() != 1) {
                            addError("sys(GETCWD) takes no additional arguments — buffer is managed automatically", expr);
                            return typeSystem->getErrorType();
                        }
                        if (firstId->name == "READLINK" && expr->arguments.size() != 2) {
                            addError("sys(READLINK, path) takes exactly one argument (the symlink path)", expr);
                            return typeSystem->getErrorType();
                        }
                    }
                }
            }
            
            // --- Safe-tier whitelist (sys() only) ---
            // The safe tier requires the first argument to be a compile-time
            // constant identifier from the curated safe syscall set.
            static const std::unordered_set<std::string> safeSyscalls = {
                // File I/O
                "READ", "WRITE", "OPEN", "CLOSE", "LSEEK", "PREAD64", "PWRITE64",
                "OPENAT", "READV", "WRITEV", "FSTAT", "NEWFSTATAT",
                // Memory
                "MMAP", "MUNMAP", "MPROTECT", "BRK", "MREMAP",
                // Directory
                "GETDENTS64", "GETCWD", "CHDIR", "MKDIR", "RMDIR", "RENAME", "RENAMEAT2",
                "UNLINK", "UNLINKAT", "SYMLINK", "READLINK",
                // File metadata
                "FCHMOD", "FCHOWN", "UTIMENSAT", "FACCESSAT",
                // Process info (read-only)
                "GETPID", "GETPPID", "GETTID", "GETUID", "GETGID", "GETEUID", "GETEGID",
                // Time
                "CLOCK_GETTIME", "CLOCK_NANOSLEEP", "NANOSLEEP",
                // Networking
                "SOCKET", "BIND", "LISTEN", "ACCEPT4", "CONNECT", "SEND", "RECV",
                "SENDTO", "RECVFROM", "SETSOCKOPT", "GETSOCKOPT", "SHUTDOWN",
                // Polling
                "POLL", "PPOLL", "EPOLL_CREATE1", "EPOLL_CTL", "EPOLL_WAIT",
                "SELECT", "PSELECT6",
                // Pipe / IPC
                "PIPE2", "DUP", "DUP2", "DUP3", "EVENTFD2",
                // Misc safe
                "IOCTL", "FCNTL", "FLOCK", "FSYNC", "FDATASYNC",
                "GETRANDOM"
            };
            
            // All known syscall constants (for full/raw tiers)
            static const std::unordered_set<std::string> allSyscalls = {
                // Everything in safeSyscalls plus dangerous ops:
                "READ", "WRITE", "OPEN", "CLOSE", "LSEEK", "PREAD64", "PWRITE64",
                "OPENAT", "READV", "WRITEV", "FSTAT", "NEWFSTATAT",
                "MMAP", "MUNMAP", "MPROTECT", "BRK", "MREMAP",
                "GETDENTS64", "GETCWD", "CHDIR", "MKDIR", "RMDIR", "RENAME", "RENAMEAT2",
                "UNLINK", "UNLINKAT", "SYMLINK", "READLINK",
                "FCHMOD", "FCHOWN", "UTIMENSAT", "FACCESSAT",
                "GETPID", "GETPPID", "GETTID", "GETUID", "GETGID", "GETEUID", "GETEGID",
                "CLOCK_GETTIME", "CLOCK_NANOSLEEP", "NANOSLEEP",
                "SOCKET", "BIND", "LISTEN", "ACCEPT4", "CONNECT", "SEND", "RECV",
                "SENDTO", "RECVFROM", "SETSOCKOPT", "GETSOCKOPT", "SHUTDOWN",
                "POLL", "PPOLL", "EPOLL_CREATE1", "EPOLL_CTL", "EPOLL_WAIT",
                "SELECT", "PSELECT6",
                "PIPE2", "DUP", "DUP2", "DUP3", "EVENTFD2",
                "IOCTL", "FCNTL", "FLOCK", "FSYNC", "FDATASYNC",
                "GETRANDOM",
                // Dangerous — require sys!! or sys!!!
                "EXIT", "EXIT_GROUP",
                "KILL", "TKILL", "TGKILL",
                "EXECVE", "EXECVEAT",
                "FORK", "CLONE", "CLONE3", "VFORK",
                "PTRACE",
                "MOUNT", "UMOUNT2",
                "REBOOT",
                "SETUID", "SETGID", "SETREUID", "SETREGID", "SETEUID", "SETEGID",
                "INIT_MODULE", "DELETE_MODULE", "FINIT_MODULE",
                "SETXATTR", "GETXATTR", "REMOVEXATTR", "LSETXATTR", "LGETXATTR", "LREMOVEXATTR",
                "FSETXATTR", "FGETXATTR", "FREMOVEXATTR", "LISTXATTR", "LLISTXATTR", "FLISTXATTR",
                "WAIT4", "WAITID",
                "PRCTL", "ARCH_PRCTL",
                "SECCOMP", "USERFAULTFD", "PERF_EVENT_OPEN",
                "SIGNALFD4", "TIMERFD_CREATE", "TIMERFD_SETTIME", "TIMERFD_GETTIME",
                "RT_SIGACTION", "RT_SIGPROCMASK", "RT_SIGRETURN", "RT_SIGPENDING",
                "SIGALTSTACK",
                "MADVISE", "MINCORE", "MLOCK", "MUNLOCK", "MLOCKALL", "MUNLOCKALL",
                "SENDMSG", "RECVMSG", "SENDMMSG", "RECVMMSG",
                "STAT", "LSTAT",
                "ACCESS", "TRUNCATE", "FTRUNCATE",
                "CHOWN", "LCHOWN", "CHMOD",
                "LINK", "LINKAT", "SYMLINKAT", "READLINKAT",
                "MKDIRAT", "MKNODAT", "UNLINKAT2",
                "STATX",
                "IO_URING_SETUP", "IO_URING_ENTER", "IO_URING_REGISTER",
                "SCHED_YIELD", "SCHED_GETAFFINITY", "SCHED_SETAFFINITY",
                "FUTEX",
                "SET_TID_ADDRESS", "SET_ROBUST_LIST", "GET_ROBUST_LIST",
                "SYSINFO", "UNAME",
                "GETRLIMIT", "SETRLIMIT", "PRLIMIT64",
                "GETRUSAGE",
                "SYSLOG",
                "INOTIFY_INIT1", "INOTIFY_ADD_WATCH", "INOTIFY_RM_WATCH"
            };
            
            // Validate first argument (syscall constant)
            if (isSafe) {
                // Safe tier: first arg MUST be an identifier from the whitelist
                IdentifierExpr* syscallId = dynamic_cast<IdentifierExpr*>(expr->arguments[0].get());
                if (!syscallId) {
                    addError("sys() requires a named syscall constant as the first argument "
                             "(e.g., READ, WRITE, OPEN). Variables and expressions are not allowed. "
                             "Use sys!!() or sys!!!() for unrestricted syscall access.", expr);
                    return typeSystem->getErrorType();
                }
                if (safeSyscalls.find(syscallId->name) == safeSyscalls.end()) {
                    if (allSyscalls.find(syscallId->name) != allSyscalls.end()) {
                        addError("'" + syscallId->name + "' is not in the safe syscall set. "
                                 "This syscall could compromise process integrity. "
                                 "Use sys!!('" + syscallId->name + "', ...) for full access, or "
                                 "sys!!!('" + syscallId->name + "', ...) for raw access.", expr);
                    } else {
                        addError("Unknown syscall constant '" + syscallId->name + "' in sys(). "
                                 "See stdlib/sys.aria for available syscall names.", expr);
                    }
                    return typeSystem->getErrorType();
                }
            }
            
            // Infer types for all arguments
            for (size_t i = 0; i < expr->arguments.size(); i++) {
                // First argument (syscall number): check for named constant FIRST
                // Named constants (READ, WRITE, etc.) are compiler builtins —
                // they are NOT variables and must NOT go through inferType().
                if (i == 0) {
                    IdentifierExpr* asId = dynamic_cast<IdentifierExpr*>(expr->arguments[0].get());
                    if (asId && (safeSyscalls.count(asId->name) || allSyscalls.count(asId->name))) {
                        // Named syscall constant — validated, codegen resolves to number
                        continue;
                    }
                    // Not a named constant — must be a numeric expression
                    if (!isSafe) {
                        // Full/raw tiers allow any integer expression as syscall number
                        Type* argType = inferType(expr->arguments[0].get());
                        if (argType->getKind() == TypeKind::ERROR) {
                            return typeSystem->getErrorType();
                        }
                        PrimitiveType* prim = dynamic_cast<PrimitiveType*>(argType);
                        if (!prim || (prim->getName() != "int64" && prim->getName() != "int32" &&
                                      prim->getName() != "uint64" && prim->getName() != "uint32")) {
                            addError(idExpr->name + "() first argument (syscall number) must be an integer type or "
                                     "a named syscall constant (e.g., READ, WRITE), got '" + argType->toString() + "'", expr);
                            return typeSystem->getErrorType();
                        }
                    }
                    // Safe tier already validated above (must be identifier in whitelist)
                    continue;
                }
                
                // Remaining args: infer type and validate
                Type* argType = inferType(expr->arguments[i].get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
                
                // Accept integer types, string, or pointer types
                PrimitiveType* prim = dynamic_cast<PrimitiveType*>(argType);
                if (prim) {
                    std::string name = prim->getName();
                    if (name == "int8" || name == "int16" || name == "int32" || name == "int64" ||
                        name == "uint8" || name == "uint16" || name == "uint32" || name == "uint64" ||
                        name == "string") {
                        continue;
                    }
                }
                PointerType* ptr = dynamic_cast<PointerType*>(argType);
                if (ptr) {
                    continue;
                }
                addError(idExpr->name + "() argument " + std::to_string(i + 1) +
                         " must be integer, string, or pointer type, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            
            // Return type depends on tier
            if (isRaw) {
                // sys!!!() returns bare int64 — no safety wrapping
                return typeSystem->getPrimitiveType("int64");
            } else {
                // Determine return type from syscall name (typed returns, v0.4.1)
                std::string syscallName;
                IdentifierExpr* firstArgId = dynamic_cast<IdentifierExpr*>(expr->arguments[0].get());
                if (firstArgId) {
                    syscallName = firstArgId->name;
                }
                
                auto retIt = syscallReturnTypes.find(syscallName);
                if (retIt != syscallReturnTypes.end() && retIt->second == SysReturnCategory::STRING_BUF) {
                    return typeSystem->getResultType(typeSystem->getPrimitiveType("string"));
                }
                // Default: Result<int64>
                return typeSystem->getResultType(typeSystem->getPrimitiveType("int64"));
            }
        }
        
        // ====================================================================
        // ARENA ALLOCATOR BUILTINS (Phase 4.2.5.2)
        // ====================================================================
        
        // Builtin: arena_new(int64) -> int64 (opaque handle)
        if (idExpr->name == "arena_new") {
            if (expr->arguments.size() != 1) {
                addError("arena_new() requires exactly one argument (capacity)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Returns int64 handle (0 = error)
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Builtin: arena_alloc(int64 handle, int64 size) -> int64 (pointer)
        if (idExpr->name == "arena_alloc") {
            if (expr->arguments.size() != 2) {
                addError("arena_alloc() requires two arguments (handle, size)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Returns int64 pointer (0 = error)
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Builtin: arena_reset(int64 handle) -> void
        if (idExpr->name == "arena_reset") {
            if (expr->arguments.size() != 1) {
                addError("arena_reset() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Returns void (no result) - but we'll return int32 for now
            return typeSystem->getPrimitiveType("int32");
        }
        
        // Builtin: arena_destroy(int64 handle) -> void
        if (idExpr->name == "arena_destroy") {
            if (expr->arguments.size() != 1) {
                addError("arena_destroy() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Returns void
            return typeSystem->getPrimitiveType("int32");
        }
        
        // Builtin: arena_get_allocated(int64 handle) -> int64
        if (idExpr->name == "arena_get_allocated") {
            if (expr->arguments.size() != 1) {
                addError("arena_get_allocated() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Builtin: arena_get_reserved(int64 handle) -> int64
        if (idExpr->name == "arena_get_reserved") {
            if (expr->arguments.size() != 1) {
                addError("arena_get_reserved() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // ====================================================================
        // ATOMIC<T> OPERATIONS - Lock-free Concurrency (P0)
        // ====================================================================
        
        // Builtin: atomic_new(initial_value) -> atomic<T>
        // Creates a new atomic variable with the given initial value
        // Type is inferred from argument: atomic_new(0i32) -> atomic<int32>
        if (idExpr->name == "atomic_new") {
            if (expr->arguments.size() != 1) {
                addError("atomic_new() requires exactly one argument (initial value)", expr);
                return typeSystem->getErrorType();
            }
            
            // Infer the type of the initial value
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // For now, return a generic type named "atomic_TYPENAME"
            // This will be handled specially in codegen
            std::string atomicTypeName = "atomic_" + argType->toString();
            return typeSystem->getGenericType(atomicTypeName);
        }
        
        // Pool allocator builtins
        if (idExpr->name == "pool_new") {
            if (expr->arguments.size() != 2) {
                addError("pool_new() requires exactly two arguments (block_size, initial_capacity)", expr);
                return typeSystem->getErrorType();
            }
            Type* blockSizeType = inferType(expr->arguments[0].get());
            Type* capacityType = inferType(expr->arguments[1].get());
            if (blockSizeType->getKind() == TypeKind::ERROR || capacityType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        if (idExpr->name == "pool_alloc") {
            if (expr->arguments.size() != 1) {
                addError("pool_alloc() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        if (idExpr->name == "pool_free") {
            if (expr->arguments.size() != 2) {
                addError("pool_free() requires exactly two arguments (handle, ptr)", expr);
                return typeSystem->getErrorType();
            }
            Type* handleType = inferType(expr->arguments[0].get());
            Type* ptrType = inferType(expr->arguments[1].get());
            if (handleType->getKind() == TypeKind::ERROR || ptrType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        if (idExpr->name == "pool_reset") {
            if (expr->arguments.size() != 1) {
                addError("pool_reset() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        if (idExpr->name == "pool_destroy") {
            if (expr->arguments.size() != 1) {
                addError("pool_destroy() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        if (idExpr->name == "pool_get_total_blocks") {
            if (expr->arguments.size() != 1) {
                addError("pool_get_total_blocks() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        if (idExpr->name == "pool_get_used_blocks") {
            if (expr->arguments.size() != 1) {
                addError("pool_get_used_blocks() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Slab allocator builtins
        if (idExpr->name == "slab_new") {
            if (expr->arguments.size() != 2) {
                addError("slab_new() requires exactly two arguments (object_size, slab_size)", expr);
                return typeSystem->getErrorType();
            }
            Type* objectSizeType = inferType(expr->arguments[0].get());
            Type* slabSizeType = inferType(expr->arguments[1].get());
            if (objectSizeType->getKind() == TypeKind::ERROR || slabSizeType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        if (idExpr->name == "slab_alloc") {
            if (expr->arguments.size() != 1) {
                addError("slab_alloc() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        if (idExpr->name == "slab_free") {
            if (expr->arguments.size() != 2) {
                addError("slab_free() requires exactly two arguments (handle, ptr)", expr);
                return typeSystem->getErrorType();
            }
            Type* handleType = inferType(expr->arguments[0].get());
            Type* ptrType = inferType(expr->arguments[1].get());
            if (handleType->getKind() == TypeKind::ERROR || ptrType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        if (idExpr->name == "slab_destroy") {
            if (expr->arguments.size() != 1) {
                addError("slab_destroy() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        if (idExpr->name == "slab_get_total_objects") {
            if (expr->arguments.size() != 1) {
                addError("slab_get_total_objects() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        if (idExpr->name == "slab_get_allocated_objects") {
            if (expr->arguments.size() != 1) {
                addError("slab_get_allocated_objects() requires exactly one argument (handle)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // ====================================================================
        // USER STACK BUILTINS (v0.4.3 — Per-scope Implicit Typed LIFO)
        // ====================================================================
        // astack()          → int64 (hidden handle; initializes scope stack with default capacity)
        // astack(capacity)  → int64 (hidden handle; initializes scope stack with given capacity)
        // apush(value)      → int64 (pushes value with auto-detected type tag; fatal on overflow)
        // apop()            → T (returns value typed to assignment context; runtime type-checked)
        // apeek()           → T (same as apop but non-destructive)

        // Builtin: astack() or astack(capacity) -> int64 (handle stored implicitly)
        if (idExpr->name == "astack") {
            if (expr->arguments.size() > 1) {
                addError("astack() takes 0 or 1 arguments (optional capacity in slots)", expr);
                return typeSystem->getErrorType();
            }
            if (expr->arguments.size() == 1) {
                Type* argType = inferType(expr->arguments[0].get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // Builtin: apush(value: any) -> int64 (fatal on overflow, return value discarded)
        if (idExpr->name == "apush") {
            if (expr->arguments.size() != 1) {
                addError("apush() requires exactly one argument (value to push)", expr);
                return typeSystem->getErrorType();
            }
            Type* valueType = inferType(expr->arguments[0].get());
            if (valueType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // Builtin: apop() -> T (type from assignment context, runtime type-checked)
        // Returns UnknownType so it is assignable to any destination type.
        // The codegen uses the destination type to determine the expected tag
        // and converts the raw i64 to the correct LLVM type.
        if (idExpr->name == "apop") {
            if (!expr->arguments.empty()) {
                addError("apop() takes no arguments (uses implicit scope stack)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getUnknownType();
        }

        // Builtin: apeek() -> T (same as apop but non-destructive)
        if (idExpr->name == "apeek") {
            if (!expr->arguments.empty()) {
                addError("apeek() takes no arguments (uses implicit scope stack)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getUnknownType();
        }

        // ====================================================================
        // WILD MEMORY BUILTINS (Phase 2.2 - Manual Memory Management)
        // ====================================================================
        // These are the primitive wild memory operations that the borrow checker
        // tracks via the ALLOCATED/FREED/MAY_FREED state machine.
        //
        // Usage:
        //   wild int8@:ptr = alloc(1024);  // Allocate 1024 bytes
        //   defer free(ptr);                // Must be freed before scope exit
        //
        // For typed allocation, use sizeof:
        //   wild int64@:array = alloc(sizeof<int64>() * count);

        // Builtin: alloc(size: int64) -> wild int8@
        // Allocates 'size' bytes of wild (manual) memory
        if (idExpr->name == "alloc") {
            if (expr->arguments.size() != 1) {
                addError("alloc() requires exactly one argument (size in bytes)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Return wild int8@ (generic byte pointer)
            // The borrow checker will track this as ALLOCATED state
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));
        }

        // Builtin: free(ptr: wild any@) -> void
        // Frees a wild pointer allocated by alloc()
        if (idExpr->name == "free") {
            if (expr->arguments.size() != 1) {
                addError("free() requires exactly one argument (pointer to free)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Check that argument is a pointer type
            if (argType->getKind() != TypeKind::POINTER) {
                addError("free() requires a pointer argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            // The borrow checker will transition this to FREED state
            return typeSystem->getPrimitiveType("void");
        }

        // Builtin: realloc(ptr: wild any@, new_size: int64) -> wild int8@
        // Reallocates a wild pointer to a new size
        if (idExpr->name == "realloc") {
            if (expr->arguments.size() != 2) {
                addError("realloc() requires exactly two arguments (ptr, new_size)", expr);
                return typeSystem->getErrorType();
            }
            Type* ptrType = inferType(expr->arguments[0].get());
            Type* sizeType = inferType(expr->arguments[1].get());
            if (ptrType->getKind() == TypeKind::ERROR || sizeType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (ptrType->getKind() != TypeKind::POINTER) {
                addError("realloc() first argument must be a pointer, got '" + ptrType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));
        }

        // ====================================================================
        // STRING LIBRARY BUILTINS (Phase 4.3)
        // ====================================================================
        
        // String creation: string_from_cstr(char*) -> string
        if (idExpr->name == "string_from_cstr") {
            if (expr->arguments.size() != 1) {
                addError("string_from_cstr() requires exactly one argument (cstr)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }
        
        // String creation: string_from_char(byte) -> string
        if (idExpr->name == "string_from_char") {
            if (expr->arguments.size() != 1) {
                addError("string_from_char() requires exactly one argument (ch)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }
        
        // String basic: string_length(string) -> int64
        if (idExpr->name == "string_length") {
            if (expr->arguments.size() != 1) {
                addError("string_length() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // String basic: string_is_empty(string) -> bool
        if (idExpr->name == "string_is_empty") {
            if (expr->arguments.size() != 1) {
                addError("string_is_empty() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // String comparison: string_equals(string, string) -> bool
        if (idExpr->name == "string_equals") {
            if (expr->arguments.size() != 2) {
                addError("string_equals() requires exactly two arguments (a, b)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // String operations: string_concat(string, string) -> string
        if (idExpr->name == "string_concat") {
            if (expr->arguments.size() != 2) {
                addError("string_concat() requires exactly two arguments (a, b)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }
        
        // String operations: string_substring(string, int64, int64) -> string
        if (idExpr->name == "string_substring") {
            if (expr->arguments.size() != 3) {
                addError("string_substring() requires exactly three arguments (str, start, end)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            Type* arg3Type = inferType(expr->arguments[2].get());
            if (arg1Type->getKind() == TypeKind::ERROR || 
                arg2Type->getKind() == TypeKind::ERROR || 
                arg3Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }
        
        // String search: string_contains(string, string) -> bool
        if (idExpr->name == "string_contains") {
            if (expr->arguments.size() != 2) {
                addError("string_contains() requires exactly two arguments (haystack, needle)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // String search: string_starts_with(string, string) -> bool
        if (idExpr->name == "string_starts_with") {
            if (expr->arguments.size() != 2) {
                addError("string_starts_with() requires exactly two arguments (str, prefix)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // String search: string_ends_with(string, string) -> bool
        if (idExpr->name == "string_ends_with") {
            if (expr->arguments.size() != 2) {
                addError("string_ends_with() requires exactly two arguments (str, suffix)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // String manipulation: string_trim(string) -> string
        if (idExpr->name == "string_trim") {
            if (expr->arguments.size() != 1) {
                addError("string_trim() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }
        
        // String manipulation: string_to_upper(string) -> string
        if (idExpr->name == "string_to_upper") {
            if (expr->arguments.size() != 1) {
                addError("string_to_upper() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }
        
        // String manipulation: string_to_lower(string) -> string
        if (idExpr->name == "string_to_lower") {
            if (expr->arguments.size() != 1) {
                addError("string_to_lower() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String conversion: string_from_int(int64) -> string
        if (idExpr->name == "string_from_int") {
            if (expr->arguments.size() != 1) {
                addError("string_from_int() requires exactly one argument (value)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String conversion: string_to_int(string) -> int64
        if (idExpr->name == "string_to_int") {
            if (expr->arguments.size() != 1) {
                addError("string_to_int() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // String conversion: string_to_hex(string) -> string
        if (idExpr->name == "string_to_hex") {
            if (expr->arguments.size() != 1) {
                addError("string_to_hex() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String manipulation: string_pad_right(string, int64, int8) -> string
        if (idExpr->name == "string_pad_right") {
            if (expr->arguments.size() != 3) {
                addError("string_pad_right() requires exactly three arguments (str, total_length, pad_char)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            Type* arg3Type = inferType(expr->arguments[2].get());
            if (arg1Type->getKind() == TypeKind::ERROR ||
                arg2Type->getKind() == TypeKind::ERROR ||
                arg3Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String manipulation: string_pad_left(string, int64, int8) -> string
        if (idExpr->name == "string_pad_left") {
            if (expr->arguments.size() != 3) {
                addError("string_pad_left() requires exactly three arguments (str, total_length, pad_char)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            Type* arg3Type = inferType(expr->arguments[2].get());
            if (arg1Type->getKind() == TypeKind::ERROR ||
                arg2Type->getKind() == TypeKind::ERROR ||
                arg3Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String manipulation: string_repeat(string, int64) -> string
        if (idExpr->name == "string_repeat") {
            if (expr->arguments.size() != 2) {
                addError("string_repeat() requires exactly two arguments (str, count)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String manipulation: string_trim_start(string) -> string
        if (idExpr->name == "string_trim_start") {
            if (expr->arguments.size() != 1) {
                addError("string_trim_start() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String manipulation: string_trim_end(string) -> string
        if (idExpr->name == "string_trim_end") {
            if (expr->arguments.size() != 1) {
                addError("string_trim_end() requires exactly one argument (str)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // String search: string_index_of(string, string) -> int64  (-1 if not found)
        if (idExpr->name == "string_index_of") {
            if (expr->arguments.size() != 2) {
                addError("string_index_of() requires exactly two arguments (haystack, needle)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // Integer hex: string_from_int_hex(int64) -> string
        if (idExpr->name == "string_from_int_hex") {
            if (expr->arguments.size() != 1) {
                addError("string_from_int_hex() requires exactly one argument (value)", expr);
                return typeSystem->getErrorType();
            }
            inferType(expr->arguments[0].get());
            return typeSystem->getPrimitiveType("string");
        }

        // String conversion: string_format_float(float64, int32) -> string
        if (idExpr->name == "string_format_float") {
            if (expr->arguments.size() != 2) {
                addError("string_format_float() requires exactly two arguments (value, precision)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // ====================================================================
        // FILE I/O BUILTINS
        // ====================================================================

        // aria_write_file_simple(path: int8*, content: int8*) -> int64
        if (idExpr->name == "aria_write_file_simple") {
            if (expr->arguments.size() != 2) {
                addError("aria_write_file_simple() requires exactly two arguments (path, content)", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // aria_file_exists(path: int8*) -> bool
        if (idExpr->name == "aria_file_exists") {
            if (expr->arguments.size() != 1) {
                addError("aria_file_exists() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // aria_delete_file_simple(path: int8*) -> int64
        if (idExpr->name == "aria_delete_file_simple") {
            if (expr->arguments.size() != 1) {
                addError("aria_delete_file_simple() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // aria_read_file_simple(path: int8*) -> string
        if (idExpr->name == "aria_read_file_simple") {
            if (expr->arguments.size() != 1) {
                addError("aria_read_file_simple() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // ====================================================================
        // TBB (Tagged Balanced Base) TYPE BUILTINS
        // ====================================================================

        // tbb64_from_int(int64) -> tbb64
        if (idExpr->name == "tbb64_from_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // ARIA-023: Compile-time validation of TBB sentinel values
            // tbb64 valid range: [INT64_MIN+1, INT64_MAX], sentinel: INT64_MIN
            ASTNode* arg = expr->arguments[0].get();
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    constexpr int64_t TBB64_SENTINEL = INT64_MIN;
                    if (val == TBB64_SENTINEL) {
                        addError("tbb64_from_int() cannot construct sentinel value (TBB64_ERR). Use tbb64_div(x, 0) to generate error state.", expr);
                        return typeSystem->getErrorType();
                    }
                }
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb64_is_err(tbb64) -> bool
        if (idExpr->name == "tbb64_is_err") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // tbb64_to_int(tbb64) -> int64
        if (idExpr->name == "tbb64_to_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_to_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // tbb32_from_int(int32) -> tbb32
        if (idExpr->name == "tbb32_from_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb32_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // ARIA-023: Compile-time validation of TBB sentinel values
            // tbb32 valid range: [-2147483647, 2147483647], sentinel: -2147483648
            ASTNode* arg = expr->arguments[0].get();
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    constexpr int64_t TBB32_SENTINEL = -2147483648LL;
                    constexpr int64_t TBB32_MAX = 2147483647LL;
                    if (val == TBB32_SENTINEL) {
                        addError("tbb32_from_int() cannot construct sentinel value -2147483648 (TBB32_ERR). Use tbb32_div(x, 0) to generate error state.", expr);
                        return typeSystem->getErrorType();
                    }
                    if (val < TBB32_SENTINEL || val > TBB32_MAX) {
                        addError("tbb32_from_int() value " + std::to_string(val) + " out of valid range [-2147483647, 2147483647]", expr);
                        return typeSystem->getErrorType();
                    }
                }
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb32_is_err(tbb32) -> bool
        if (idExpr->name == "tbb32_is_err") {
            if (expr->arguments.size() != 1) {
                addError("tbb32_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // tbb32_to_int(tbb32) -> int32
        if (idExpr->name == "tbb32_to_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb32_to_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }

        // tbb16_from_int(int16) -> tbb16
        if (idExpr->name == "tbb16_from_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb16_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // ARIA-023: Compile-time validation of TBB sentinel values
            // tbb16 valid range: [-32767, 32767], sentinel: -32768
            ASTNode* arg = expr->arguments[0].get();
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    constexpr int64_t TBB16_SENTINEL = -32768;
                    constexpr int64_t TBB16_MAX = 32767;
                    if (val == TBB16_SENTINEL) {
                        addError("tbb16_from_int() cannot construct sentinel value -32768 (TBB16_ERR). Use tbb16_div(x, 0) to generate error state.", expr);
                        return typeSystem->getErrorType();
                    }
                    if (val < TBB16_SENTINEL || val > TBB16_MAX) {
                        addError("tbb16_from_int() value " + std::to_string(val) + " out of valid range [-32767, 32767]", expr);
                        return typeSystem->getErrorType();
                    }
                }
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb16_is_err(tbb16) -> bool
        if (idExpr->name == "tbb16_is_err") {
            if (expr->arguments.size() != 1) {
                addError("tbb16_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // tbb16_to_int(tbb16) -> int16
        if (idExpr->name == "tbb16_to_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb16_to_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int16");
        }

        // tbb8_from_int(int8) -> tbb8
        if (idExpr->name == "tbb8_from_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb8_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // ARIA-023: Compile-time validation of TBB sentinel values
            // tbb8 valid range: [-127, 127], sentinel: -128
            ASTNode* arg = expr->arguments[0].get();
            std::optional<int64_t> constVal;
            // Check for direct literal
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    constVal = std::get<int64_t>(lit->value);
                }
            }
            // Check for unary minus with literal (e.g., -128)
            else if (arg->type == ASTNode::NodeType::UNARY_OP) {
                UnaryExpr* unary = static_cast<UnaryExpr*>(arg);
                if (unary->op.type == frontend::TokenType::TOKEN_MINUS && unary->operand) {
                    if (unary->operand->type == ASTNode::NodeType::LITERAL) {
                        LiteralExpr* lit = static_cast<LiteralExpr*>(unary->operand.get());
                        if (std::holds_alternative<int64_t>(lit->value)) {
                            constVal = -std::get<int64_t>(lit->value);
                        }
                    }
                }
            }
            if (constVal.has_value()) {
                int64_t val = constVal.value();
                if (val == -128) {
                    addError("tbb8_from_int() cannot construct sentinel value -128 (TBB8_ERR). Use tbb8_div(x, 0) to generate error state.", expr);
                    return typeSystem->getErrorType();
                }
                if (val < -127 || val > 127) {
                    addError("tbb8_from_int() value " + std::to_string(val) + " out of valid range [-127, 127]", expr);
                    return typeSystem->getErrorType();
                }
            }
            return typeSystem->getPrimitiveType("tbb8");
        }

        // tbb8_is_err(tbb8) -> bool
        if (idExpr->name == "tbb8_is_err") {
            if (expr->arguments.size() != 1) {
                addError("tbb8_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // tbb8_to_int(tbb8) -> int8
        if (idExpr->name == "tbb8_to_int") {
            if (expr->arguments.size() != 1) {
                addError("tbb8_to_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int8");
        }

        // ====================================================================
        // TBB WIDENING INTRINSICS (ARIA-018: Sentinel-Preserving Widening)
        // ====================================================================
        // Standard sign extension would heal ERR sentinel:
        //   tbb8(-128) → sext → tbb16(-128) [VALID in tbb16, not ERR!]
        // These intrinsics preserve the ERR state across widening operations
        
        // tbb16_from_tbb8(tbb8) -> tbb16
        if (idExpr->name == "tbb16_from_tbb8") {
            if (expr->arguments.size() != 1) {
                addError("tbb16_from_tbb8() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "tbb8") {
                addError("tbb16_from_tbb8() requires tbb8 argument, got " + argType->toString(), expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }
        
        // tbb32_from_tbb8(tbb8) -> tbb32
        if (idExpr->name == "tbb32_from_tbb8") {
            if (expr->arguments.size() != 1) {
                addError("tbb32_from_tbb8() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "tbb8") {
                addError("tbb32_from_tbb8() requires tbb8 argument, got " + argType->toString(), expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }
        
        // tbb32_from_tbb16(tbb16) -> tbb32
        if (idExpr->name == "tbb32_from_tbb16") {
            if (expr->arguments.size() != 1) {
                addError("tbb32_from_tbb16() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "tbb16") {
                addError("tbb32_from_tbb16() requires tbb16 argument, got " + argType->toString(), expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }
        
        // tbb64_from_tbb8(tbb8) -> tbb64
        if (idExpr->name == "tbb64_from_tbb8") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_from_tbb8() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "tbb8") {
                addError("tbb64_from_tbb8() requires tbb8 argument, got " + argType->toString(), expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }
        
        // tbb64_from_tbb16(tbb16) -> tbb64
        if (idExpr->name == "tbb64_from_tbb16") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_from_tbb16() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "tbb16") {
                addError("tbb64_from_tbb16() requires tbb16 argument, got " + argType->toString(), expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }
        
        // tbb64_from_tbb32(tbb32) -> tbb64
        if (idExpr->name == "tbb64_from_tbb32") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_from_tbb32() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "tbb32") {
                addError("tbb64_from_tbb32() requires tbb32 argument, got " + argType->toString(), expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // ====================================================================
        // NUMERIC TYPE CONSTRUCTORS (Session 2: frac, tfp, vec9, tmatrix, ttensor)
        // ====================================================================
        
        // Fraction constructors: make_frac8, make_frac16, make_frac32, make_frac64
        if (idExpr->name == "make_frac8" || idExpr->name == "make_frac16" ||
            idExpr->name == "make_frac32" || idExpr->name == "make_frac64") {
            if (expr->arguments.size() != 3) {
                addError(idExpr->name + "() requires exactly 3 arguments (whole, numerator, denominator)", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            
            // Return the appropriate frac type
            std::string typeName = idExpr->name.substr(5);  // Remove "make_" prefix
            return typeSystem->getPrimitiveType(typeName);
        }
        
        // Tri-state floating point constructors: make_tfp32, make_tfp64
        if (idExpr->name == "make_tfp32" || idExpr->name == "make_tfp64") {
            if (expr->arguments.size() != 2) {
                addError(idExpr->name + "() requires exactly 2 arguments (exponent, mantissa)", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            
            // Return the appropriate tfp type
            std::string typeName = idExpr->name.substr(5);  // Remove "make_" prefix
            return typeSystem->getPrimitiveType(typeName);
        }
        
        // Vector constructor: make_vec9
        if (idExpr->name == "make_vec9") {
            if (expr->arguments.size() != 9) {
                addError("make_vec9() requires exactly 9 arguments (9 int32 elements)", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            
            // Return vec9 (TBB-inspired vector struct, not dvec9 float vector)
            return typeSystem->getPrimitiveType("vec9");
        }
        
        // vec9 operations
        if (idExpr->name == "vec9_add" || idExpr->name == "vec9_sub") {
            if (expr->arguments.size() != 2) {
                addError(idExpr->name + "() requires exactly 2 vec9 arguments", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments - both must be vec9
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
                // Check if argument is vec9
                if (PrimitiveType* primType = dynamic_cast<PrimitiveType*>(argType)) {
                    if (primType->getName() != "vec9") {
                        addError(idExpr->name + "() requires vec9 arguments", expr);
                        return typeSystem->getErrorType();
                    }
                } else {
                    addError(idExpr->name + "() requires vec9 arguments", expr);
                    return typeSystem->getErrorType();
                }
            }
            
            // Return vec9
            return typeSystem->getPrimitiveType("vec9");
        }
        
        if (idExpr->name == "vec9_scale") {
            if (expr->arguments.size() != 2) {
                addError("vec9_scale() requires exactly 2 arguments (vec9, int32)", expr);
                return typeSystem->getErrorType();
            }
            
            // Check first argument is vec9
            Type* arg1Type = inferType(expr->arguments[0].get());
            if (arg1Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (PrimitiveType* primType = dynamic_cast<PrimitiveType*>(arg1Type)) {
                if (primType->getName() != "vec9") {
                    addError("vec9_scale() first argument must be vec9", expr);
                    return typeSystem->getErrorType();
                }
            } else {
                addError("vec9_scale() first argument must be vec9", expr);
                return typeSystem->getErrorType();
            }
            
            // Check second argument is int32
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Return vec9
            return typeSystem->getPrimitiveType("vec9");
        }
        
        if (idExpr->name == "vec9_dot") {
            if (expr->arguments.size() != 2) {
                addError("vec9_dot() requires exactly 2 vec9 arguments", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments - both must be vec9
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
                // Check if argument is vec9
                if (PrimitiveType* primType = dynamic_cast<PrimitiveType*>(argType)) {
                    if (primType->getName() != "vec9") {
                        addError("vec9_dot() requires vec9 arguments", expr);
                        return typeSystem->getErrorType();
                    }
                } else {
                    addError("vec9_dot() requires vec9 arguments", expr);
                    return typeSystem->getErrorType();
                }
            }
            
            // Return int32 (dot product is scalar)
            return typeSystem->getPrimitiveType("int32");
        }
        
        // Tensor matrix constructor: make_tmatrix
        if (idExpr->name == "make_tmatrix") {
            if (expr->arguments.size() != 4) {
                addError("make_tmatrix() requires exactly 4 arguments (rows, cols, rank, data)", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            
            return typeSystem->getPrimitiveType("tmatrix");
        }
        
        // Tensor constructor: make_ttensor
        if (idExpr->name == "make_ttensor") {
            if (expr->arguments.size() != 11) {
                addError("make_ttensor() requires exactly 11 arguments (9 shape dims + rank + data)", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            
            return typeSystem->getPrimitiveType("ttensor");
        }
        
        // tmatrix_identity(rows, cols) -> tmatrix
        // Creates an identity matrix with specified dimensions
        if (idExpr->name == "tmatrix_identity") {
            if (expr->arguments.size() != 2) {
                addError("tmatrix_identity() requires exactly 2 arguments (rows, cols)", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check arguments - should be integers
            for (const auto& arg : expr->arguments) {
                Type* argType = inferType(arg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
            }
            
            return typeSystem->getPrimitiveType("tmatrix");
        }
        
        // ttensor_zeros(shape) -> ttensor
        // Creates a zeroed 9D tensor with specified shape
        if (idExpr->name == "ttensor_zeros") {
            if (expr->arguments.size() != 1) {
                addError("ttensor_zeros() requires exactly 1 argument (9D shape vector or array)", expr);
                return typeSystem->getErrorType();
            }
            
            // Type check argument
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            return typeSystem->getPrimitiveType("ttensor");
        }

        // ====================================================================
        // MAP/DICTIONARY RUNTIME FUNCTIONS
        // ====================================================================
        
        // map_new(key_size, value_size) -> wild void->
        if (idExpr->name == "map_new") {
            if (expr->arguments.size() != 2) {
                addError("map_new() requires exactly 2 arguments (key_size, value_size)", expr);
                return typeSystem->getErrorType();
            }
            
            // Return wild void-> (opaque map handle)
            Type* voidType = typeSystem->getPrimitiveType("void");
            return typeSystem->getPointerType(voidType);
        }
        
        // map_insert(map, key_ptr, value_ptr) -> void
        if (idExpr->name == "map_insert") {
            if (expr->arguments.size() != 3) {
                addError("map_insert() requires exactly 3 arguments (map, key_ptr, value_ptr)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }
        
        // map_get(map, key_ptr) -> wild void->
        if (idExpr->name == "map_get") {
            if (expr->arguments.size() != 2) {
                addError("map_get() requires exactly 2 arguments (map, key_ptr)", expr);
                return typeSystem->getErrorType();
            }
            
            Type* voidType = typeSystem->getPrimitiveType("void");
            return typeSystem->getPointerType(voidType);
        }
        
        // map_has(map, key_ptr) -> bool
        if (idExpr->name == "map_has") {
            if (expr->arguments.size() != 2) {
                addError("map_has() requires exactly 2 arguments (map, key_ptr)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // map_remove(map, key_ptr) -> void
        if (idExpr->name == "map_remove") {
            if (expr->arguments.size() != 2) {
                addError("map_remove() requires exactly 2 arguments (map, key_ptr)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }
        
        // map_length(map) -> int64
        if (idExpr->name == "map_length") {
            if (expr->arguments.size() != 1) {
                addError("map_length() requires exactly 1 argument (map)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // ====================================================================
        // OPTIONAL/RESULT HELPER FUNCTIONS
        // ====================================================================
        
        // get_optional_value(condition) -> int64?
        if (idExpr->name == "get_optional_value") {
            if (expr->arguments.size() != 1) {
                addError("get_optional_value() requires exactly 1 argument (condition)", expr);
                return typeSystem->getErrorType();
            }
            
            Type* int64Type = typeSystem->getPrimitiveType("int64");
            return typeSystem->getOptionalType(int64Type);
        }

        // ====================================================================
        // ARIA-018: TBB SENTINEL-PRESERVING WIDENING
        // ====================================================================

        // tbb_widen_8_to_16(tbb8) -> tbb16
        if (idExpr->name == "tbb_widen_8_to_16") {
            if (expr->arguments.size() != 1) {
                addError("tbb_widen_8_to_16() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb_widen_8_to_32(tbb8) -> tbb32
        if (idExpr->name == "tbb_widen_8_to_32") {
            if (expr->arguments.size() != 1) {
                addError("tbb_widen_8_to_32() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb_widen_8_to_64(tbb8) -> tbb64
        if (idExpr->name == "tbb_widen_8_to_64") {
            if (expr->arguments.size() != 1) {
                addError("tbb_widen_8_to_64() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb_widen_16_to_32(tbb16) -> tbb32
        if (idExpr->name == "tbb_widen_16_to_32") {
            if (expr->arguments.size() != 1) {
                addError("tbb_widen_16_to_32() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb_widen_16_to_64(tbb16) -> tbb64
        if (idExpr->name == "tbb_widen_16_to_64") {
            if (expr->arguments.size() != 1) {
                addError("tbb_widen_16_to_64() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb_widen_32_to_64(tbb32) -> tbb64
        if (idExpr->name == "tbb_widen_32_to_64") {
            if (expr->arguments.size() != 1) {
                addError("tbb_widen_32_to_64() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // ====================================================================
        // TBB ARITHMETIC OPERATIONS
        // ====================================================================

        // tbb64_add(tbb64, tbb64) -> tbb64
        if (idExpr->name == "tbb64_add") {
            if (expr->arguments.size() != 2) {
                addError("tbb64_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb64_sub(tbb64, tbb64) -> tbb64
        if (idExpr->name == "tbb64_sub") {
            if (expr->arguments.size() != 2) {
                addError("tbb64_sub() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb64_mul(tbb64, tbb64) -> tbb64
        if (idExpr->name == "tbb64_mul") {
            if (expr->arguments.size() != 2) {
                addError("tbb64_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb64_div(tbb64, tbb64) -> tbb64
        if (idExpr->name == "tbb64_div") {
            if (expr->arguments.size() != 2) {
                addError("tbb64_div() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb64_neg(tbb64) -> tbb64
        if (idExpr->name == "tbb64_neg") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb64_abs(tbb64) -> tbb64
        if (idExpr->name == "tbb64_abs") {
            if (expr->arguments.size() != 1) {
                addError("tbb64_abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb64");
        }

        // tbb32_add(tbb32, tbb32) -> tbb32
        if (idExpr->name == "tbb32_add") {
            if (expr->arguments.size() != 2) {
                addError("tbb32_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb32_sub(tbb32, tbb32) -> tbb32
        if (idExpr->name == "tbb32_sub") {
            if (expr->arguments.size() != 2) {
                addError("tbb32_sub() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb32_mul(tbb32, tbb32) -> tbb32
        if (idExpr->name == "tbb32_mul") {
            if (expr->arguments.size() != 2) {
                addError("tbb32_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb32_div(tbb32, tbb32) -> tbb32
        if (idExpr->name == "tbb32_div") {
            if (expr->arguments.size() != 2) {
                addError("tbb32_div() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb32_neg(tbb32) -> tbb32
        if (idExpr->name == "tbb32_neg") {
            if (expr->arguments.size() != 1) {
                addError("tbb32_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb32_abs(tbb32) -> tbb32
        if (idExpr->name == "tbb32_abs") {
            if (expr->arguments.size() != 1) {
                addError("tbb32_abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb32");
        }

        // tbb16_add(tbb16, tbb16) -> tbb16
        if (idExpr->name == "tbb16_add") {
            if (expr->arguments.size() != 2) {
                addError("tbb16_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb16_sub(tbb16, tbb16) -> tbb16
        if (idExpr->name == "tbb16_sub") {
            if (expr->arguments.size() != 2) {
                addError("tbb16_sub() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb16_mul(tbb16, tbb16) -> tbb16
        if (idExpr->name == "tbb16_mul") {
            if (expr->arguments.size() != 2) {
                addError("tbb16_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb16_div(tbb16, tbb16) -> tbb16
        if (idExpr->name == "tbb16_div") {
            if (expr->arguments.size() != 2) {
                addError("tbb16_div() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb16_neg(tbb16) -> tbb16
        if (idExpr->name == "tbb16_neg") {
            if (expr->arguments.size() != 1) {
                addError("tbb16_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb16_abs(tbb16) -> tbb16
        if (idExpr->name == "tbb16_abs") {
            if (expr->arguments.size() != 1) {
                addError("tbb16_abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb16");
        }

        // tbb8_add(tbb8, tbb8) -> tbb8
        if (idExpr->name == "tbb8_add") {
            if (expr->arguments.size() != 2) {
                addError("tbb8_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb8");
        }

        // tbb8_sub(tbb8, tbb8) -> tbb8
        if (idExpr->name == "tbb8_sub") {
            if (expr->arguments.size() != 2) {
                addError("tbb8_sub() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb8");
        }

        // tbb8_mul(tbb8, tbb8) -> tbb8
        if (idExpr->name == "tbb8_mul") {
            if (expr->arguments.size() != 2) {
                addError("tbb8_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb8");
        }

        // tbb8_div(tbb8, tbb8) -> tbb8
        if (idExpr->name == "tbb8_div") {
            if (expr->arguments.size() != 2) {
                addError("tbb8_div() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb8");
        }

        // tbb8_neg(tbb8) -> tbb8
        if (idExpr->name == "tbb8_neg") {
            if (expr->arguments.size() != 1) {
                addError("tbb8_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb8");
        }

        // tbb8_abs(tbb8) -> tbb8
        if (idExpr->name == "tbb8_abs") {
            if (expr->arguments.size() != 1) {
                addError("tbb8_abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tbb8");
        }

        // ====================================================================
        // EXOTIC TYPE CONSTRUCTORS (LBIM, Fixed Point, Fractional)
        // ====================================================================
        
        // uint1024_from_int(int64) -> uint1024
        if (idExpr->name == "uint1024_from_int") {
            if (expr->arguments.size() != 1) {
                addError("uint1024_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("uint1024");
        }
        
        // int1024_from_int(int64) -> int1024
        if (idExpr->name == "int1024_from_int") {
            if (expr->arguments.size() != 1) {
                addError("int1024_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int1024");
        }
        
        // ====================================================================
        // LBIM EXPONENTIATION — int*_pow(base, exp_int64) -> int*
        // Binary exponentiation, returns ERR on overflow
        // ====================================================================
        
        if (idExpr->name == "int128_pow" || idExpr->name == "uint128_pow") {
            if (expr->arguments.size() != 2) {
                addError(idExpr->name + "() requires exactly two arguments (base, exponent)", expr);
                return typeSystem->getErrorType();
            }
            inferType(expr->arguments[0].get());
            inferType(expr->arguments[1].get());
            return typeSystem->getPrimitiveType(idExpr->name.substr(0, idExpr->name.find("_pow")));
        }
        
        if (idExpr->name == "int256_pow" || idExpr->name == "uint256_pow") {
            if (expr->arguments.size() != 2) {
                addError(idExpr->name + "() requires exactly two arguments (base, exponent)", expr);
                return typeSystem->getErrorType();
            }
            inferType(expr->arguments[0].get());
            inferType(expr->arguments[1].get());
            return typeSystem->getPrimitiveType(idExpr->name.substr(0, idExpr->name.find("_pow")));
        }
        
        if (idExpr->name == "int512_pow" || idExpr->name == "uint512_pow") {
            if (expr->arguments.size() != 2) {
                addError(idExpr->name + "() requires exactly two arguments (base, exponent)", expr);
                return typeSystem->getErrorType();
            }
            inferType(expr->arguments[0].get());
            inferType(expr->arguments[1].get());
            return typeSystem->getPrimitiveType(idExpr->name.substr(0, idExpr->name.find("_pow")));
        }
        
        if (idExpr->name == "int1024_pow" || idExpr->name == "uint1024_pow") {
            if (expr->arguments.size() != 2) {
                addError(idExpr->name + "() requires exactly two arguments (base, exponent)", expr);
                return typeSystem->getErrorType();
            }
            inferType(expr->arguments[0].get());
            inferType(expr->arguments[1].get());
            return typeSystem->getPrimitiveType(idExpr->name.substr(0, idExpr->name.find("_pow")));
        }
        
        // ====================================================================
        // STATIC TYPE CONSTANTS (Type.CONSTANT syntax)
        // Parser transforms Type.MEMBER → Type_MEMBER() zero-arg call
        // ====================================================================
        
        // TBB ERR sentinels
        if (idExpr->name == "tbb8_ERR") {
            if (expr->arguments.size() != 0) { addError("tbb8.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("tbb8");
        }
        if (idExpr->name == "tbb16_ERR") {
            if (expr->arguments.size() != 0) { addError("tbb16.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("tbb16");
        }
        if (idExpr->name == "tbb32_ERR") {
            if (expr->arguments.size() != 0) { addError("tbb32.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("tbb32");
        }
        if (idExpr->name == "tbb64_ERR") {
            if (expr->arguments.size() != 0) { addError("tbb64.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("tbb64");
        }
        
        // Balanced type ERR sentinels
        if (idExpr->name == "trit_ERR") {
            if (expr->arguments.size() != 0) { addError("trit.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("trit");
        }
        if (idExpr->name == "nit_ERR") {
            if (expr->arguments.size() != 0) { addError("nit.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("nit");
        }
        if (idExpr->name == "tryte_ERR") {
            if (expr->arguments.size() != 0) { addError("tryte.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("tryte");
        }
        if (idExpr->name == "nyte_ERR") {
            if (expr->arguments.size() != 0) { addError("nyte.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // fix256 constants
        if (idExpr->name == "fix256_ERR") {
            if (expr->arguments.size() != 0) { addError("fix256.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("fix256");
        }
        if (idExpr->name == "fix256_MAX") {
            if (expr->arguments.size() != 0) { addError("fix256.MAX takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("fix256");
        }
        if (idExpr->name == "fix256_EPSILON") {
            if (expr->arguments.size() != 0) { addError("fix256.EPSILON takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("fix256");
        }
        
        // LBIM ERR sentinels (for overflow/error detection in multi-limb arithmetic)
        if (idExpr->name == "int256_ERR") {
            if (expr->arguments.size() != 0) { addError("int256.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("int256");
        }
        if (idExpr->name == "int512_ERR") {
            if (expr->arguments.size() != 0) { addError("int512.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("int512");
        }
        if (idExpr->name == "int1024_ERR") {
            if (expr->arguments.size() != 0) { addError("int1024.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("int1024");
        }
        if (idExpr->name == "uint256_ERR") {
            if (expr->arguments.size() != 0) { addError("uint256.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("uint256");
        }
        if (idExpr->name == "uint512_ERR") {
            if (expr->arguments.size() != 0) { addError("uint512.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("uint512");
        }
        if (idExpr->name == "uint1024_ERR") {
            if (expr->arguments.size() != 0) { addError("uint1024.ERR takes no arguments", expr); return typeSystem->getErrorType(); }
            return typeSystem->getPrimitiveType("uint1024");
        }
        
        // fix256_from_int(int64) -> fix256
        if (idExpr->name == "fix256_from_int") {
            if (expr->arguments.size() != 1) {
                addError("fix256_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("fix256");
        }
        
        // fix256_from_float(flt64) -> fix256
        if (idExpr->name == "fix256_from_float") {
            if (expr->arguments.size() != 1) {
                addError("fix256_from_float() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("fix256");
        }
        
        // fix256_to_int(fix256) -> int64
        if (idExpr->name == "fix256_to_int") {
            if (expr->arguments.size() != 1) {
                addError("fix256_to_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // fix256_to_float(fix256) -> flt64
        if (idExpr->name == "fix256_to_float") {
            if (expr->arguments.size() != 1) {
                addError("fix256_to_float() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }
        
        // trit_from_int(int64) -> trit (balanced ternary: -1, 0, 1)
        if (idExpr->name == "trit_from_int") {
            if (expr->arguments.size() != 1) {
                addError("trit_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Validate range at compile time if possible
            ASTNode* arg = expr->arguments[0].get();
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    if (val < -1 || val > 1) {
                        addError("trit_from_int() value " + std::to_string(val) + " out of range [-1, 1]", expr);
                        return typeSystem->getErrorType();
                    }
                }
            }
            return typeSystem->getPrimitiveType("trit");
        }
        
        // nit_from_int(int64) -> nit (balanced nonary: -4 to +4)
        if (idExpr->name == "nit_from_int") {
            if (expr->arguments.size() != 1) {
                addError("nit_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Validate range at compile time if possible
            ASTNode* arg = expr->arguments[0].get();
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    if (val < -4 || val > 4) {
                        addError("nit_from_int() value " + std::to_string(val) + " out of range [-4, 4]", expr);
                        return typeSystem->getErrorType();
                    }
                }
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // tryte_from_int(int64) -> tryte (10 balanced ternary digits packed in uint16)
        if (idExpr->name == "tryte_from_int") {
            if (expr->arguments.size() != 1) {
                addError("tryte_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Validate range: 3^10 = 59049, balanced: -29524 to +29524
            ASTNode* arg = expr->arguments[0].get();
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    if (val < -29524 || val > 29524) {
                        addError("tryte_from_int() value " + std::to_string(val) + " out of range [-29524, 29524]", expr);
                        return typeSystem->getErrorType();
                    }
                }
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // nyte_from_int(int64) -> nyte (5 balanced nonary digits packed in uint16)
        if (idExpr->name == "nyte_from_int") {
            if (expr->arguments.size() != 1) {
                addError("nyte_from_int() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Validate range: 9^5 = 59049, balanced: -29524 to +29524
            ASTNode* arg = expr->arguments[0].get();
            if (arg->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(arg);
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    if (val < -29524 || val > 29524) {
                        addError("nyte_from_int() value " + std::to_string(val) + " out of range [-29524, 29524]", expr);
                        return typeSystem->getErrorType();
                    }
                }
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // ====================================================================
        // TRIT/NIT LOGIC AND ARITHMETIC INTRINSICS
        // ====================================================================
        // Trit: Single balanced ternary digit (-1, 0, 1, ERR=-128)
        // Nit: Single balanced nonary digit (-4 to +4, ERR=-128)
        
        // trit_add(trit, trit) -> trit
        if (idExpr->name == "trit_add") {
            if (expr->arguments.size() != 2) {
                addError("trit_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("trit");
        }
        
        // trit_mul(trit, trit) -> trit
        if (idExpr->name == "trit_mul") {
            if (expr->arguments.size() != 2) {
                addError("trit_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("trit");
        }
        
        // trit_neg(trit) -> trit
        if (idExpr->name == "trit_neg") {
            if (expr->arguments.size() != 1) {
                addError("trit_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("trit");
        }
        
        // trit_and(trit, trit) -> trit (Kleene three-valued logic)
        if (idExpr->name == "trit_and") {
            if (expr->arguments.size() != 2) {
                addError("trit_and() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("trit");
        }
        
        // trit_or(trit, trit) -> trit (Kleene three-valued logic)
        if (idExpr->name == "trit_or") {
            if (expr->arguments.size() != 2) {
                addError("trit_or() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("trit");
        }
        
        // trit_not(trit) -> trit (Kleene three-valued logic)
        if (idExpr->name == "trit_not") {
            if (expr->arguments.size() != 1) {
                addError("trit_not() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("trit");
        }
        
        // trit_is_err(trit) -> bool
        if (idExpr->name == "trit_is_err") {
            if (expr->arguments.size() != 1) {
                addError("trit_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // nit_and(nit, nit) -> nit (Nonary AND - extended Kleene logic)
        if (idExpr->name == "nit_and") {
            if (expr->arguments.size() != 2) {
                addError("nit_and() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // nit_or(nit, nit) -> nit (Nonary OR - extended Kleene logic)
        if (idExpr->name == "nit_or") {
            if (expr->arguments.size() != 2) {
                addError("nit_or() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // nit_is_err(nit) -> bool
        if (idExpr->name == "nit_is_err") {
            if (expr->arguments.size() != 1) {
                addError("nit_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // ====================================================================
        // NIT ARITHMETIC INTRINSICS
        // ====================================================================
        
        // nit_add(nit, nit) -> nit
        if (idExpr->name == "nit_add") {
            if (expr->arguments.size() != 2) {
                addError("nit_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // nit_sub(nit, nit) -> nit
        if (idExpr->name == "nit_sub") {
            if (expr->arguments.size() != 2) {
                addError("nit_sub() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // nit_mul(nit, nit) -> nit
        if (idExpr->name == "nit_mul") {
            if (expr->arguments.size() != 2) {
                addError("nit_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // nit_div(nit, nit) -> nit
        if (idExpr->name == "nit_div") {
            if (expr->arguments.size() != 2) {
                addError("nit_div() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // nit_neg(nit) -> nit
        if (idExpr->name == "nit_neg") {
            if (expr->arguments.size() != 1) {
                addError("nit_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // nit_abs(nit) -> nit
        if (idExpr->name == "nit_abs") {
            if (expr->arguments.size() != 1) {
                addError("nit_abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nit");
        }
        
        // ====================================================================
        // TRYTE ARITHMETIC INTRINSICS
        // ====================================================================
        
        // tryte_add(tryte, tryte) -> tryte
        if (idExpr->name == "tryte_add") {
            if (expr->arguments.size() != 2) {
                addError("tryte_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // tryte_sub(tryte, tryte) -> tryte
        if (idExpr->name == "tryte_sub") {
            if (expr->arguments.size() != 2) {
                addError("tryte_sub() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // tryte_mul(tryte, tryte) -> tryte
        if (idExpr->name == "tryte_mul") {
            if (expr->arguments.size() != 2) {
                addError("tryte_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // tryte_div(tryte, tryte) -> tryte
        if (idExpr->name == "tryte_div") {
            if (expr->arguments.size() != 2) {
                addError("tryte_div() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // tryte_mod(tryte, tryte) -> tryte
        if (idExpr->name == "tryte_mod") {
            if (expr->arguments.size() != 2) {
                addError("tryte_mod() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // tryte_neg(tryte) -> tryte
        if (idExpr->name == "tryte_neg") {
            if (expr->arguments.size() != 1) {
                addError("tryte_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // tryte_abs(tryte) -> tryte
        if (idExpr->name == "tryte_abs") {
            if (expr->arguments.size() != 1) {
                addError("tryte_abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // tryte_is_err(tryte) -> bool
        if (idExpr->name == "tryte_is_err") {
            if (expr->arguments.size() != 1) {
                addError("tryte_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // ====================================================================
        // NYTE ARITHMETIC INTRINSICS
        // ====================================================================
        
        // nyte_add(nyte, nyte) -> nyte
        if (idExpr->name == "nyte_add") {
            if (expr->arguments.size() != 2) {
                addError("nyte_add() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_sub(nyte, nyte) -> nyte
        if (idExpr->name == "nyte_sub") {
            if (expr->arguments.size() != 2) {
                addError("nyte_sub() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_mul(nyte, nyte) -> nyte
        if (idExpr->name == "nyte_mul") {
            if (expr->arguments.size() != 2) {
                addError("nyte_mul() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_div(nyte, nyte) -> nyte
        if (idExpr->name == "nyte_div") {
            if (expr->arguments.size() != 2) {
                addError("nyte_div() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_mod(nyte, nyte) -> nyte
        if (idExpr->name == "nyte_mod") {
            if (expr->arguments.size() != 2) {
                addError("nyte_mod() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_neg(nyte) -> nyte
        if (idExpr->name == "nyte_neg") {
            if (expr->arguments.size() != 1) {
                addError("nyte_neg() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_abs(nyte) -> nyte
        if (idExpr->name == "nyte_abs") {
            if (expr->arguments.size() != 1) {
                addError("nyte_abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_is_err(nyte) -> bool
        if (idExpr->name == "nyte_is_err") {
            if (expr->arguments.size() != 1) {
                addError("nyte_is_err() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // ====================================================================
        // TRYTE/NYTE CONVERSION INTRINSICS
        // ====================================================================
        
        // tryte_from_balanced(int32) -> tryte
        if (idExpr->name == "tryte_from_balanced") {
            if (expr->arguments.size() != 1) {
                addError("tryte_from_balanced() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // nyte_from_balanced(int32) -> nyte
        if (idExpr->name == "nyte_from_balanced") {
            if (expr->arguments.size() != 1) {
                addError("nyte_from_balanced() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // tryte_to_balanced(tryte) -> int32
        if (idExpr->name == "tryte_to_balanced") {
            if (expr->arguments.size() != 1) {
                addError("tryte_to_balanced() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        // nyte_to_balanced(nyte) -> int32
        if (idExpr->name == "nyte_to_balanced") {
            if (expr->arguments.size() != 1) {
                addError("nyte_to_balanced() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        // tryte_to_nyte(tryte) -> nyte
        if (idExpr->name == "tryte_to_nyte") {
            if (expr->arguments.size() != 1) {
                addError("tryte_to_nyte() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("nyte");
        }
        
        // nyte_to_tryte(nyte) -> tryte
        if (idExpr->name == "nyte_to_tryte") {
            if (expr->arguments.size() != 1) {
                addError("nyte_to_tryte() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("tryte");
        }
        
        // ====================================================================
        // INTEGER CONVERSION BUILTINS
        // ====================================================================
        
        // int8_from_int64(int64) -> int8
        if (idExpr->name == "int8_from_int64") {
            if (expr->arguments.size() != 1) {
                addError("int8_from_int64() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int8");
        }
        
        // int16_from_int64(int64) -> int16
        if (idExpr->name == "int16_from_int64") {
            if (expr->arguments.size() != 1) {
                addError("int16_from_int64() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int16");
        }
        
        // int32_from_int64(int64) -> int32
        if (idExpr->name == "int32_from_int64") {
            if (expr->arguments.size() != 1) {
                addError("int32_from_int64() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        // int64_from_int32(int32) -> int64
        if (idExpr->name == "int64_from_int32") {
            if (expr->arguments.size() != 1) {
                addError("int64_from_int32() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // int64_from_int8(int8) -> int64
        if (idExpr->name == "int64_from_int8") {
            if (expr->arguments.size() != 1) {
                addError("int64_from_int8() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }
        
        // ====================================================================
        // MEMORY MANAGEMENT BUILTINS
        // ====================================================================
        
        // ====================================================================
        // EXPLICIT SAFETY BYPASS
        // ====================================================================

        // raw(Result<T>) -> T  — Explicitly bypass "no checky no val"
        // Purpose: Extract .value without checking .is_error first.
        //          Use when you have already validated externally, or in
        //          performance-critical paths where the check is elsewhere.
        // This is the ONLY sanctioned way to skip the is_error check.
        // Example: int8:val = raw(buffer_read(buf, idx));
        if (idExpr->name == "raw") {
            if (expr->arguments.size() != 1) {
                addError("raw() requires exactly one argument (Result<T>)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->getKind() != TypeKind::RESULT) {
                addError("raw() argument must be Result<T> — got '" + argType->toString() + "'.\n"
                         "  raw() extracts .value without an is_error check.\n"
                         "  If you have a plain value already, raw() is not needed.",
                         expr);
                return typeSystem->getErrorType();
            }
            // Bypass the KNOWN_SUCCESS gate — return inner T directly
            ResultType* resType = static_cast<ResultType*>(argType);
            return resType->getValueType();
        }

        // drop(wild T@) -> void - Free wild pointer
        if (idExpr->name == "drop") {
            if (expr->arguments.size() != 1) {
                addError("drop() requires exactly one argument (wild pointer)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // TODO: Verify argument is a wild pointer
            return typeSystem->getPrimitiveType("void");
        }

        // sleep_ms(int64) -> void
        // Suspends execution for given number of milliseconds
        if (idExpr->name == "sleep_ms") {
            if (expr->arguments.size() != 1) {
                addError("sleep_ms() requires exactly one argument (milliseconds)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }

        // exit(int32) -> noreturn (void for type purposes)
        // Terminates the process with the given exit code.
        // Only valid in 'main' and 'failsafe' — the two program endpoints.
        if (idExpr->name == "exit") {
            if (currentFunctionName != "main" && currentFunctionName != "failsafe") {
                addError("exit() can only be called from 'main' or 'failsafe'. "
                         "Use pass()/fail() to return from regular functions.", expr);
                return typeSystem->getErrorType();
            }
            if (expr->arguments.size() != 1) {
                addError("exit() requires exactly one argument (exit code: int32)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            PrimitiveType* argPrim = dynamic_cast<PrimitiveType*>(argType);
            if (!argPrim || argPrim->getName() != "int32") {
                addError("exit() requires an int32 exit code, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            // In failsafe, exit code must be > 0 (error path — exit(0) is wrong)
            if (currentFunctionName == "failsafe") {
                if (expr->arguments[0]->type == ASTNode::NodeType::LITERAL) {
                    LiteralExpr* lit = static_cast<LiteralExpr*>(expr->arguments[0].get());
                    if (std::holds_alternative<int64_t>(lit->value)) {
                        int64_t val = std::get<int64_t>(lit->value);
                        if (val <= 0) {
                            addError("exit() in failsafe must have a positive exit code (got " +
                                     std::to_string(val) + "). failsafe is an error path.", expr);
                        }
                    }
                }
            }
            return typeSystem->getPrimitiveType("void");
        }

        // env_get(string) -> string
        // Returns the value of an environment variable, or empty string if not set.
        if (idExpr->name == "env_get") {
            if (expr->arguments.size() != 1) {
                addError("env_get() requires exactly one argument (name: string)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "string") {
                addError("env_get() requires a string argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // sort_lines(string) -> string
        // Sorts newline-separated lines lexicographically.
        if (idExpr->name == "sort_lines") {
            if (expr->arguments.size() != 1) {
                addError("sort_lines() requires exactly one argument (content: string)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            if (argType->toString() != "string") {
                addError("sort_lines() requires a string argument, got '" + argType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // ====================================================================
        // COLLECTION BUILTINS (Phase 6.2)
        // ====================================================================

        // array_new(element_size: int64) -> int8@
        // Creates a new empty array with given element size
        if (idExpr->name == "array_new") {
            if (expr->arguments.size() != 1) {
                addError("array_new() requires exactly one argument (element_size)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Returns a pointer to AriaArray (int8@ as generic pointer)
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));
        }

        // array_length(array: ptr) -> int64
        // Returns the number of elements in the array
        if (idExpr->name == "array_length") {
            if (expr->arguments.size() != 1) {
                addError("array_length() requires exactly one argument (array)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // array_push(array: ptr, value: ptr) -> int32
        // Appends an element to the array (returns 0 on success)
        if (idExpr->name == "array_push") {
            if (expr->arguments.size() != 2) {
                addError("array_push() requires exactly two arguments (array, value)", expr);
                return typeSystem->getErrorType();
            }
            Type* arrType = inferType(expr->arguments[0].get());
            Type* valType = inferType(expr->arguments[1].get());
            if (arrType->getKind() == TypeKind::ERROR || valType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }

        // array_get(array: ptr, index: int64) -> int8@
        // Returns pointer to element at index
        if (idExpr->name == "array_get") {
            if (expr->arguments.size() != 2) {
                addError("array_get() requires exactly two arguments (array, index)", expr);
                return typeSystem->getErrorType();
            }
            Type* arrType = inferType(expr->arguments[0].get());
            Type* idxType = inferType(expr->arguments[1].get());
            if (arrType->getKind() == TypeKind::ERROR || idxType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));
        }

        // array_set(array: ptr, index: int64, value: ptr) -> int32
        // Sets element at index (returns 0 on success)
        if (idExpr->name == "array_set") {
            if (expr->arguments.size() != 3) {
                addError("array_set() requires exactly three arguments (array, index, value)", expr);
                return typeSystem->getErrorType();
            }
            Type* arrType = inferType(expr->arguments[0].get());
            Type* idxType = inferType(expr->arguments[1].get());
            Type* valType = inferType(expr->arguments[2].get());
            if (arrType->getKind() == TypeKind::ERROR || idxType->getKind() == TypeKind::ERROR ||
                valType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }

        // array_pop(array: ptr) -> int8@
        // Removes and returns pointer to last element
        if (idExpr->name == "array_pop") {
            if (expr->arguments.size() != 1) {
                addError("array_pop() requires exactly one argument (array)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));
        }

        // ====================================================================
        // MAP BUILTINS
        // ====================================================================

        // map_new(key_size: int64, value_size: int64) -> int8@
        // Creates a new empty map with given key and value sizes
        if (idExpr->name == "map_new") {
            if (expr->arguments.size() != 2) {
                addError("map_new() requires exactly two arguments (key_size, value_size)", expr);
                return typeSystem->getErrorType();
            }
            Type* keySizeType = inferType(expr->arguments[0].get());
            Type* valSizeType = inferType(expr->arguments[1].get());
            if (keySizeType->getKind() == TypeKind::ERROR || valSizeType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Returns a pointer to AriaMap (int8@ as generic pointer)
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));
        }

        // map_length(map: ptr) -> int64
        // Returns the number of key-value pairs in the map
        if (idExpr->name == "map_length") {
            if (expr->arguments.size() != 1) {
                addError("map_length() requires exactly one argument (map)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // map_insert(map: ptr, key: ptr, value: ptr) -> void
        // Inserts or updates a key-value pair in the map
        if (idExpr->name == "map_insert") {
            if (expr->arguments.size() != 3) {
                addError("map_insert() requires exactly three arguments (map, key, value)", expr);
                return typeSystem->getErrorType();
            }
            Type* mapType = inferType(expr->arguments[0].get());
            Type* keyType = inferType(expr->arguments[1].get());
            Type* valType = inferType(expr->arguments[2].get());
            if (mapType->getKind() == TypeKind::ERROR || keyType->getKind() == TypeKind::ERROR ||
                valType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }

        // map_get(map: ptr, key: ptr) -> int8@
        // Returns pointer to value associated with key, or nullptr if not found
        if (idExpr->name == "map_get") {
            if (expr->arguments.size() != 2) {
                addError("map_get() requires exactly two arguments (map, key)", expr);
                return typeSystem->getErrorType();
            }
            Type* mapType = inferType(expr->arguments[0].get());
            Type* keyType = inferType(expr->arguments[1].get());
            if (mapType->getKind() == TypeKind::ERROR || keyType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));
        }

        // map_has(map: ptr, key: ptr) -> bool
        // Returns true if the key exists in the map
        if (idExpr->name == "map_has") {
            if (expr->arguments.size() != 2) {
                addError("map_has() requires exactly two arguments (map, key)", expr);
                return typeSystem->getErrorType();
            }
            Type* mapType = inferType(expr->arguments[0].get());
            Type* keyType = inferType(expr->arguments[1].get());
            if (mapType->getKind() == TypeKind::ERROR || keyType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // map_remove(map: ptr, key: ptr) -> void
        // Removes a key-value pair from the map
        if (idExpr->name == "map_remove") {
            if (expr->arguments.size() != 2) {
                addError("map_remove() requires exactly two arguments (map, key)", expr);
                return typeSystem->getErrorType();
            }
            Type* mapType = inferType(expr->arguments[0].get());
            Type* keyType = inferType(expr->arguments[1].get());
            if (mapType->getKind() == TypeKind::ERROR || keyType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }

        // ====================================================================
        // FILE I/O BUILTINS (Phase 4.2)
        // ====================================================================
        
        // readFile(path: string) -> result<string>
        // Returns file content or error
        if (idExpr->name == "readFile") {
            if (expr->arguments.size() != 1) {
                addError("readFile() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("string"));
        }
        
        // writeFile(path: string, content: string) -> result<void>
        // Returns success/error only
        if (idExpr->name == "writeFile") {
            if (expr->arguments.size() != 2) {
                addError("writeFile() requires exactly two arguments (path, content)", expr);
                return typeSystem->getErrorType();
            }
            Type* pathType = inferType(expr->arguments[0].get());
            Type* contentType = inferType(expr->arguments[1].get());
            if (pathType->getKind() == TypeKind::ERROR || contentType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // For void results, we use result<void> (represented as ResultType with nullptr valueType or a VoidType)
            // For now, we'll return result<int64> for compatibility (0 = success in val, err = NIL)
            return typeSystem->getResultType(typeSystem->getPrimitiveType("int64"));
        }
        
        // allocate(size: int32) -> buffer@
        // Allocates wild memory and returns pointer to buffer
        if (idExpr->name == "allocate") {
            if (expr->arguments.size() != 1) {
                addError("allocate() requires exactly one argument (size)", expr);
                return typeSystem->getErrorType();
            }
            Type* sizeType = inferType(expr->arguments[0].get());
            if (sizeType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Return buffer type (represents void* wild memory pointer)
            return typeSystem->getPrimitiveType("buffer");
        }
        
        // fileExists(path: string) -> result<bool>
        // Returns whether file exists or error
        if (idExpr->name == "fileExists") {
            if (expr->arguments.size() != 1) {
                addError("fileExists() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("bool"));
        }
        
        // fileSize(path: string) -> result<int64>
        // Returns file size in bytes or error
        if (idExpr->name == "fileSize") {
            if (expr->arguments.size() != 1) {
                addError("fileSize() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("int64"));
        }
        
        // deleteFile(path: string) -> result<void>
        // Returns success/error only
        if (idExpr->name == "deleteFile") {
            if (expr->arguments.size() != 1) {
                addError("deleteFile() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // For void results, return result<int64> for compatibility (0 = success in val, err = NIL)
            return typeSystem->getResultType(typeSystem->getPrimitiveType("int64"));
        }
        
        // ====================================================================
        // PATH OPERATION BUILTINS (Phase 4.2 - Path Utils)
        // ====================================================================
        
        // pathAbsolute(path: string) -> result<string>
        // Converts a path to absolute form or returns error
        if (idExpr->name == "pathAbsolute") {
            if (expr->arguments.size() != 1) {
                addError("pathAbsolute() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("string"));
        }
        
        // pathDirname(path: string) -> result<string>
        // Returns the directory portion of a path or error
        if (idExpr->name == "pathDirname") {
            if (expr->arguments.size() != 1) {
                addError("pathDirname() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("string"));
        }
        
        // pathBasename(path: string) -> result<string>
        // Returns the filename portion of a path or error
        if (idExpr->name == "pathBasename") {
            if (expr->arguments.size() != 1) {
                addError("pathBasename() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("string"));
        }
        
        // pathJoin(dir: string, name: string) -> result<string>
        // Joins two path components or returns error
        if (idExpr->name == "pathJoin") {
            if (expr->arguments.size() != 2) {
                addError("pathJoin() requires exactly two arguments (dir, name)", expr);
                return typeSystem->getErrorType();
            }
            Type* dirType = inferType(expr->arguments[0].get());
            Type* nameType = inferType(expr->arguments[1].get());
            if (dirType->getKind() == TypeKind::ERROR || nameType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("string"));
        }
        
        // pathIsAbsolute(path: string) -> result<bool>
        // Checks if a path is absolute or returns error
        if (idExpr->name == "pathIsAbsolute") {
            if (expr->arguments.size() != 1) {
                addError("pathIsAbsolute() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getResultType(typeSystem->getPrimitiveType("bool"));
        }

        // ====================================================================
        // FILE UFCS BUILTINS (for File.open(), stream.read(), etc.)
        // ====================================================================

        // File_open(path: string, mode: string) -> ptr (AriaStream*)
        // Static method: File.open("path", "r")
        if (idExpr->name == "File_open") {
            if (expr->arguments.size() != 2) {
                addError("File_open() requires exactly two arguments (path, mode)", expr);
                return typeSystem->getErrorType();
            }
            Type* pathType = inferType(expr->arguments[0].get());
            Type* modeType = inferType(expr->arguments[1].get());
            if (pathType->getKind() == TypeKind::ERROR || modeType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPointerType(typeSystem->getPrimitiveType("int8"));  // ptr
        }

        // File_read_line(stream: ptr) -> string
        // Instance method: stream.read_line()
        if (idExpr->name == "File_read_line") {
            if (expr->arguments.size() != 1) {
                addError("File_read_line() requires exactly one argument (stream)", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // File_read_bytes(stream: ptr, buffer: ptr, size: int64) -> int64
        if (idExpr->name == "File_read_bytes") {
            if (expr->arguments.size() != 3) {
                addError("File_read_bytes() requires exactly three arguments (stream, buffer, size)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // File_write(stream: ptr, str: string) -> int64
        // Instance method: stream.write("text")
        if (idExpr->name == "File_write") {
            if (expr->arguments.size() != 2) {
                addError("File_write() requires exactly two arguments (stream, str)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // File_write_bytes(stream: ptr, data: ptr, size: int64) -> int64
        if (idExpr->name == "File_write_bytes") {
            if (expr->arguments.size() != 3) {
                addError("File_write_bytes() requires exactly three arguments (stream, data, size)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // File_close(stream: ptr) -> void
        // Instance method: stream.close()
        if (idExpr->name == "File_close") {
            if (expr->arguments.size() != 1) {
                addError("File_close() requires exactly one argument (stream)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("void");
        }

        // File_eof(stream: ptr) -> bool
        // Instance method: stream.eof()
        if (idExpr->name == "File_eof") {
            if (expr->arguments.size() != 1) {
                addError("File_eof() requires exactly one argument (stream)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // File_flush(stream: ptr) -> int32
        if (idExpr->name == "File_flush") {
            if (expr->arguments.size() != 1) {
                addError("File_flush() requires exactly one argument (stream)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }

        // File_seek(stream: ptr, offset: int64, whence: int32) -> int32
        if (idExpr->name == "File_seek") {
            if (expr->arguments.size() != 3) {
                addError("File_seek() requires exactly three arguments (stream, offset, whence)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }

        // File_tell(stream: ptr) -> int64
        if (idExpr->name == "File_tell") {
            if (expr->arguments.size() != 1) {
                addError("File_tell() requires exactly one argument (stream)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // File_read(path: string) -> string (static, reads whole file)
        if (idExpr->name == "File_read") {
            if (expr->arguments.size() != 1) {
                addError("File_read() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("string");
        }

        // File_write_all(path: string, content: string) -> int64
        if (idExpr->name == "File_write_all") {
            if (expr->arguments.size() != 2) {
                addError("File_write_all() requires exactly two arguments (path, content)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // File_delete(path: string) -> int64
        if (idExpr->name == "File_delete") {
            if (expr->arguments.size() != 1) {
                addError("File_delete() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // File_exists(path: string) -> bool
        if (idExpr->name == "File_exists") {
            if (expr->arguments.size() != 1) {
                addError("File_exists() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("bool");
        }

        // File_size(path: string) -> int64
        if (idExpr->name == "File_size") {
            if (expr->arguments.size() != 1) {
                addError("File_size() requires exactly one argument (path)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int64");
        }

        // ====================================================================
        // MATH LIBRARY BUILTINS (Phase 4.4)
        // ====================================================================
        
        // Basic math functions
        if (idExpr->name == "abs") {
            if (expr->arguments.size() != 1) {
                addError("abs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Return same type as input
            return argType;
        }
        
        if (idExpr->name == "min" || idExpr->name == "max") {
            if (expr->arguments.size() != 2) {
                addError(idExpr->name + "() requires exactly two arguments", expr);
                return typeSystem->getErrorType();
            }
            Type* arg1Type = inferType(expr->arguments[0].get());
            Type* arg2Type = inferType(expr->arguments[1].get());
            if (arg1Type->getKind() == TypeKind::ERROR || arg2Type->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // Both arguments should be same type
            return arg1Type;
        }
        
        // Rounding functions - all return flt64
        if (idExpr->name == "floor" || idExpr->name == "ceil" || 
            idExpr->name == "round" || idExpr->name == "trunc") {
            if (expr->arguments.size() != 1) {
                addError(idExpr->name + "() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }
        
        // Exponential and logarithmic functions - all take/return flt64
        if (idExpr->name == "sqrt" || idExpr->name == "exp" || 
            idExpr->name == "log" || idExpr->name == "log10" || idExpr->name == "log2") {
            if (expr->arguments.size() != 1) {
                addError(idExpr->name + "() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }
        
        if (idExpr->name == "pow") {
            if (expr->arguments.size() != 2) {
                addError("pow() requires exactly two arguments (base, exponent)", expr);
                return typeSystem->getErrorType();
            }
            Type* baseType = inferType(expr->arguments[0].get());
            Type* expType = inferType(expr->arguments[1].get());
            if (baseType->getKind() == TypeKind::ERROR || expType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }
        
        // Trigonometric functions - all take/return flt64
        if (idExpr->name == "sin" || idExpr->name == "cos" || idExpr->name == "tan" ||
            idExpr->name == "asin" || idExpr->name == "acos" || idExpr->name == "atan") {
            if (expr->arguments.size() != 1) {
                addError(idExpr->name + "() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }
        
        if (idExpr->name == "atan2") {
            if (expr->arguments.size() != 2) {
                addError("atan2() requires exactly two arguments (y, x)", expr);
                return typeSystem->getErrorType();
            }
            Type* yType = inferType(expr->arguments[0].get());
            Type* xType = inferType(expr->arguments[1].get());
            if (yType->getKind() == TypeKind::ERROR || xType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }
        
        // Absolute value and float modulo - two libm functions not covered above
        if (idExpr->name == "fabs") {
            if (expr->arguments.size() != 1) {
                addError("fabs() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }

        if (idExpr->name == "fmod") {
            if (expr->arguments.size() != 2) {
                addError("fmod() requires exactly two arguments (x, y)", expr);
                return typeSystem->getErrorType();
            }
            Type* xType = inferType(expr->arguments[0].get());
            Type* yType = inferType(expr->arguments[1].get());
            if (xType->getKind() == TypeKind::ERROR || yType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }

        // Mathematical constants - no arguments, return flt64
        if (idExpr->name == "PI" || idExpr->name == "E" || idExpr->name == "TAU") {
            if (expr->arguments.size() != 0) {
                addError(idExpr->name + "() is a constant and takes no arguments", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("flt64");
        }
    }
    
    // Infer callee type (should be function type or callable object)
    Type* calleeType = inferType(expr->callee.get());
    
    if (calleeType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Get the function declaration (if this is a direct function call)
    FuncDeclStmt* funcDecl = nullptr;
    if (expr->callee->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* idExpr = static_cast<IdentifierExpr*>(expr->callee.get());
        Symbol* funcSymbol = symbolTable->lookupSymbol(idExpr->name);
        if (funcSymbol && funcSymbol->kind == SymbolKind::FUNCTION) {
            funcDecl = funcSymbol->getFuncDecl();
        }
    }
    
    // Check if this is a generic function call
    if (funcDecl && !funcDecl->genericParams.empty() && genericResolver && monomorphizer) {
        // Infer argument types
        std::vector<Type*> argTypes;
        for (const auto& arg : expr->arguments) {
            Type* argType = inferType(arg.get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            argTypes.push_back(argType);
        }
        
        // Check for explicit type arguments (turbofish syntax)
        TypeSubstitution substitution;
        if (!expr->explicitTypeArgs.empty()) {
            // Explicit type arguments provided: identity::<int32>(42)
            substitution = genericResolver->resolveExplicitTypeArgs(funcDecl, expr->explicitTypeArgs);
        } else {
            // Type inference: identity(42)
            substitution = genericResolver->inferTypeArgs(funcDecl, expr, argTypes);
        }
        
        // Check for errors in type resolution
        if (genericResolver->hasErrors()) {
            for (const auto& err : genericResolver->getErrors()) {
                addError(err.message, expr);
            }
            genericResolver->clearErrors();
            return typeSystem->getErrorType();
        }
        
        // Request specialization
        Specialization* spec = monomorphizer->requestSpecialization(funcDecl, substitution);
        if (!spec) {
            if (monomorphizer->hasErrors()) {
                for (const auto& err : monomorphizer->getErrors()) {
                    addError(err.message, expr);
                }
            }
            return typeSystem->getErrorType();
        }
        
        // Store the mangled name in the CallExpr for IR generation
        expr->specializedMangledName = spec->mangledName;
        
        // Transitive monomorphization: walk the specialized body to find and
        // specialize any nested generic function calls (e.g. complex_add calls
        // complex_is_err internally — both need to be specialized).
        if (spec->funcDecl) {
            analyzeSpecializedBody(spec->funcDecl, spec->substitution);
        }
        
        // Resolve the specialized function's return type
        // The specialized function has concrete types substituted
        if (spec->funcDecl && spec->funcDecl->returnType) {
            Type* returnType = resolveTypeNode(spec->funcDecl->returnType.get());
            if (returnType && returnType->getKind() != TypeKind::ERROR) {
                // Aria functions return Result<T>, but pipeline calls automatically unwrap
                // Skip Result wrapping for pipeline-generated calls
                if (!expr->isPipelineCall && returnType->getKind() != TypeKind::RESULT) {
                    return typeSystem->getResultType(returnType);
                }
                return returnType;
            }
        }
        
        // Fallback: return unknown type if resolution fails
        return typeSystem->getUnknownType();
    }
    
    // Non-generic function call or generics not available
    // Validate argument count and types
    
    // If we have a function type, validate arguments
    if (calleeType->getKind() == TypeKind::FUNCTION) {
        FunctionType* funcType = static_cast<FunctionType*>(calleeType);
        
        // Check argument count
        size_t expectedCount = funcType->getParamCount();
        size_t actualCount = expr->arguments.size();
        
        if (!funcType->isVariadicFunction() && actualCount != expectedCount) {
            addError("Function expects " + std::to_string(expectedCount) + 
                    " argument(s), but " + std::to_string(actualCount) + " provided", expr);
            return typeSystem->getErrorType();
        }
        
        if (funcType->isVariadicFunction() && actualCount < expectedCount) {
            addError("Variadic function expects at least " + std::to_string(expectedCount) + 
                    " argument(s), but " + std::to_string(actualCount) + " provided", expr);
            return typeSystem->getErrorType();
        }
        
        // Check argument types
        const auto& paramTypes = funcType->getParamTypes();
        for (size_t i = 0; i < expectedCount && i < actualCount; ++i) {
            Type* argType = inferType(expr->arguments[i].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Pipeline operators automatically unwrap Result types
            // When Result<T> is passed where T is expected, we auto-unwrap
            Type* effectiveArgType = argType;
            if (argType->getKind() == TypeKind::RESULT) {
                ResultType* resultType = static_cast<ResultType*>(argType);
                effectiveArgType = resultType->getValueType();
            }
            
            // Check if argument type is assignable to parameter type
            // P1.5: Allow automatic void* ↔ wild T-> conversions at FFI boundaries
            bool ffiPointerConversion = canConvertFFIPointer(effectiveArgType, paramTypes[i]);

            // Allow implicit array-to-pointer decay: T[N] → T@ (like C array decay)
            bool arrayToPointerCoercion = false;
            if (effectiveArgType->getKind() == TypeKind::ARRAY &&
                paramTypes[i]->getKind() == TypeKind::POINTER) {
                ArrayType* arrType = static_cast<ArrayType*>(effectiveArgType);
                PointerType* ptrType = static_cast<PointerType*>(paramTypes[i]);
                if (arrType->getElementType()->isAssignableTo(ptrType->getPointeeType())) {
                    arrayToPointerCoercion = true;
                }
            }

            // v0.2.36: Allow concrete type -> dyn Trait coercion if type implements the trait
            bool dynTraitCoercion = false;
            if (paramTypes[i]->getKind() == TypeKind::DYN_TRAIT) {
                DynTraitType* dynType = static_cast<DynTraitType*>(paramTypes[i]);
                Symbol* traitSym = symbolTable->lookupSymbol(dynType->getTraitName());
                if (traitSym && traitSym->kind == SymbolKind::TRAIT) {
                    // Check if the concrete type has an impl for this trait
                    std::string concreteTypeName = effectiveArgType->toString();
                    for (const auto* implDecl : traitSym->getImplDecls()) {
                        if (implDecl->typeName == concreteTypeName) {
                            dynTraitCoercion = true;
                            break;
                        }
                    }
                }
            }

            if (!effectiveArgType->isAssignableTo(paramTypes[i]) && !ffiPointerConversion && !arrayToPointerCoercion && !dynTraitCoercion) {
                addError("Argument " + std::to_string(i + 1) + " has type '" + 
                        argType->toString() + "', but function expects '" + 
                        paramTypes[i]->toString() + "'", expr->arguments[i].get());
                // Continue checking other arguments to report all errors
            }
        }
        
        // Return the function's return type
        // For pipeline calls, return type is already unwrapped (no Result wrapping)
        Type* returnType = funcType->getReturnType();
        
        // Pipeline operators work with unwrapped values - don't re-wrap in Result
        if (expr->isPipelineCall) {
            return returnType;
        }
        
        return returnType;
    }
    
    // If callee type is not a function, it's an error
    if (calleeType->getKind() != TypeKind::UNKNOWN) {
        addError("Cannot call non-function type '" + calleeType->toString() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    // Unknown type - return unknown (for cases where type hasn't been inferred yet)
    return typeSystem->getUnknownType();
}

// ============================================================================
// Array Index Type Inference
// ============================================================================

Type* TypeChecker::inferIndexExpr(IndexExpr* expr) {
    // Infer array type
    Type* arrayType = inferType(expr->array.get());
    
    if (arrayType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Check if this is array slicing (index is a range)
    if (expr->index->type == ASTNode::NodeType::RANGE) {
        // Array slicing: arr[start..end] returns an array of same element type
        RangeExpr* rangeExpr = static_cast<RangeExpr*>(expr->index.get());
        
        // Type check the range expression
        Type* rangeType = inferType(rangeExpr);
        if (rangeType->getKind() == TypeKind::ERROR) {
            return typeSystem->getErrorType();
        }
        
        // Verify we're slicing an array (pointer type)
        if (arrayType->getKind() != TypeKind::POINTER) {
            addError("Cannot slice non-array type '" + arrayType->toString() + "'", expr);
            return typeSystem->getErrorType();
        }
        
        // Slicing returns the same type (pointer to element type)
        // arr[0..5] where arr is int64[] returns int64[]
        return arrayType;
    }
    
    // Regular indexing: arr[i]
    // Infer index type
    Type* indexType = inferType(expr->index.get());
    
    if (indexType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Check that index is integer type
    PrimitiveType* indexPrim = dynamic_cast<PrimitiveType*>(indexType);
    if (!indexPrim || (indexPrim->getName().find("int") != 0 && indexPrim->getName().find("uint") != 0)) {
        addError("Array index must be integer type, got '" + indexType->toString() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    // Vectors support index access: vec[i]
    if (arrayType->getKind() == TypeKind::VECTOR) {
        VectorType* vecType = static_cast<VectorType*>(arrayType);
        return vecType->getComponentType();
    }
    
    // SIMD types support element access: simd<T, N>[i] -> T
    if (arrayType->getKind() == TypeKind::SIMD) {
        SimdType* simdType = static_cast<SimdType*>(arrayType);
        return simdType->getElementType();
    }
    
    // Arrays support index access: int32[10][i] -> int32
    if (arrayType->getKind() == TypeKind::ARRAY) {
        ArrayType* arrType = static_cast<ArrayType*>(arrayType);
        return arrType->getElementType();
    }
    
    // Arrays are represented as pointer types (legacy compatibility)
    // Extract element type from pointer
    if (arrayType->getKind() == TypeKind::POINTER) {
        PointerType* ptrType = static_cast<PointerType*>(arrayType);
        return ptrType->getPointeeType();
    }
    
    // Not an array/pointer/vector type
    addError("Cannot index non-array type '" + arrayType->toString() + "'", expr);
    return typeSystem->getErrorType();
}

// ============================================================================
// Member Access Type Inference
// ============================================================================

Type* TypeChecker::inferMemberAccessExpr(MemberAccessExpr* expr) {
    // Check for GPU intrinsic calls (gpu.thread_id(), gpu.sync_threads(), etc.)
    // GPU/PTX Backend Phase 4: GPU Intrinsics
    if (expr->object->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* identExpr = static_cast<IdentifierExpr*>(expr->object.get());
        if (identExpr->name == "gpu") {
            // All GPU intrinsics return int32 (except sync_threads which is void/statement-level)
            // thread_id(), block_id(), block_dim(), grid_dim() all return int32
            return typeSystem->getPrimitiveType("int32");
        }
    }
    
    // Check if this is module member access (module.function or module.symbol)
    // This must come before type inference to handle MODULE symbols correctly
    if (expr->object->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* identExpr = static_cast<IdentifierExpr*>(expr->object.get());
        Symbol* baseSym = symbolTable->resolveSymbol(identExpr->name);
        
        // If the base is a MODULE symbol, resolve member from the module
        if (baseSym && baseSym->isModule()) {
            Module* module = baseSym->getModuleRef();
            if (!module) {
                addError("Module '" + identExpr->name + "' has invalid module reference", expr);
                return typeSystem->getErrorType();
            }
            
            // Look up member in module exports
            const auto& exports = module->getExports();
            auto it = exports.find(expr->member);
            if (it != exports.end()) {
                // Found exported symbol - return its type
                return it->second.symbol->type;
            }
            
            // Member not found in module exports
            addError("Module '" + identExpr->name + "' has no exported member '" + 
                    expr->member + "'", expr);
            return typeSystem->getErrorType();
        }
        
        // Also check for enum variant access (EnumName.VARIANT)
        std::string fullName = identExpr->name + "." + expr->member;
        Symbol* variantSym = symbolTable->resolveSymbol(fullName);
        if (variantSym) {
            // Found enum variant - return its type (should be int64)
            return variantSym->type;
        }
    }
    
    // Infer object type
    Type* objectType = inferType(expr->object.get());
    
    if (objectType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Handle vector component access (.x, .y, .z for vec2/vec3, [i] for all)
    if (objectType->getKind() == TypeKind::VECTOR) {
        VectorType* vecType = static_cast<VectorType*>(objectType);
        int dimension = vecType->getDimension();
        
        // Check valid component names
        if (expr->member == "x" && dimension >= 1) {
            return vecType->getComponentType();
        }
        if (expr->member == "y" && dimension >= 2) {
            return vecType->getComponentType();
        }
        if (expr->member == "z" && dimension >= 3) {
            return vecType->getComponentType();
        }
        
        // Invalid component for this vector dimension
        std::ostringstream oss;
        oss << "vec" << dimension << " has no component '" << expr->member << "'";
        if (dimension == 2) {
            oss << " (valid: x, y)";
        } else if (dimension == 3) {
            oss << " (valid: x, y, z)";
        } else {
            oss << " (use index access [i] for vec9)";
        }
        addError(oss.str(), expr);
        return typeSystem->getErrorType();
    }
    
    // Handle Result type member access (.is_error, .value, .error)
    // Reference: runtime/result/result.cpp, include/runtime/result.h
    // ENFORCEMENT: "No checky no val" - must check .is_error before accessing .value/.error
    if (objectType->getKind() == TypeKind::RESULT) {
        ResultType* resultType = static_cast<ResultType*>(objectType);
        std::string varName = getVariableName(expr->object.get());
        
        // .is_error -> bool
        // IMPORTANT: Accessing .is_error marks this Result as checked
        if (expr->member == "is_error") {
            if (!varName.empty()) {
                markResultChecked(varName);
            }
            return typeSystem->getPrimitiveType("bool");
        }
        
        // .value -> T (the wrapped value type)
        // ENFORCEMENT: Cannot access .value without first checking .is_error
        // Phase 2: Must KNOW is_error == false, not just that we checked it
        if (expr->member == "value") {
            if (!varName.empty()) {
                ResultCheckState::State state = currentResultState.getState(varName);
                if (state != ResultCheckState::State::KNOWN_SUCCESS) {
                    addError("Cannot access .value without checking .is_error first (no checky no val).\n"
                            "  Help: Add a check: if (!" + varName + ".is_error) { ... }\n"
                            "  Or use unwrap operator: value = " + varName + " ? default_value;",
                            expr);
                    return typeSystem->getErrorType();
                }
            }
            return resultType->getValueType();
        }
        
        // .error -> error pointer (represented as int64 for now)
        // ENFORCEMENT: Cannot access .error without first checking .is_error
        // Phase 2: Can access if we checked OR know is_error == true
        // TODO: Create proper Error type when error handling is expanded
        if (expr->member == "error") {
            if (!varName.empty()) {
                ResultCheckState::State state = currentResultState.getState(varName);
                // Can access error if we checked (for logging) or know it's an error
                if (state == ResultCheckState::State::UNCHECKED) {
                    addError("Cannot access .error without checking .is_error first (no checky no val).\n"
                            "  Help: Add a check: if (" + varName + ".is_error) { ... }",
                            expr);
                    return typeSystem->getErrorType();
                }
            }
            // Error code type: int32 (matches failsafe tbb32 convention and fail() error codes)
            return typeSystem->getPrimitiveType("int32");
        }
        
        // Invalid member
        addError("result<T> has no member '" + expr->member + 
                "' (valid: is_error, value, error)", expr);
        return typeSystem->getErrorType();
    }
    
    // For safe navigation (?.) or regular access on optional types,
    // unwrap the optional to get the inner type and proceed with member access.
    // Safe navigation returns Optional<fieldType>; regular access returns fieldType directly.
    if (objectType->getKind() == TypeKind::OPTIONAL) {
        OptionalType* optType = static_cast<OptionalType*>(objectType);
        Type* innerType = optType->getWrappedType();
        
        if (innerType->getKind() == TypeKind::STRUCT) {
            StructType* structType = static_cast<StructType*>(innerType);
            const auto& fields = structType->getFields();
            
            for (const auto& field : fields) {
                if (field.name == expr->member) {
                    // Safe navigation (?.) returns Optional<fieldType>
                    if (expr->isSafeNavigation) {
                        return typeSystem->getOptionalType(field.type);
                    }
                    // Regular access on optional — return field type directly
                    return field.type;
                }
            }
            
            // Member not found
            addError("Struct '" + structType->getName() + "' has no member named '" +
                    expr->member + "'", expr);
            return typeSystem->getErrorType();
        }
        
        addError("Safe navigation requires optional of struct type, got '" +
                objectType->toString() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    // Handle struct member access
    if (objectType->getKind() == TypeKind::STRUCT) {
        StructType* structType = static_cast<StructType*>(objectType);
        const auto& fields = structType->getFields();
        
        // Look up member in struct fields
        for (const auto& field : fields) {
            if (field.name == expr->member) {
                // For safe navigation, result type is the same for now
                // TODO: Return optional<field.type> when optional types are implemented
                return field.type;
            }
        }

        // Member not found - provide enhanced error with suggestions
        std::string bestMatch;
        size_t bestDistance = SIZE_MAX;
        size_t maxDistance = std::max(size_t(2), expr->member.length() / 2);

        std::string availableFields;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) availableFields += ", ";
            availableFields += fields[i].name;

            // Check for similar field name
            size_t dist = levenshteinDistance(expr->member, fields[i].name);
            if (dist < bestDistance && dist <= maxDistance) {
                bestDistance = dist;
                bestMatch = fields[i].name;
            }
        }

        std::string errorMsg = "Struct '" + structType->getName() + "' has no member named '" +
                expr->member + "'";

        if (!bestMatch.empty()) {
            errorMsg += ". Did you mean '" + bestMatch + "'?";
        }

        if (!availableFields.empty()) {
            errorMsg += "\n  Available fields: " + availableFields;
        }

        addError(errorMsg, expr);
        return typeSystem->getErrorType();
    }
    
    // P1-3 Phase 3: Handle<T> member access
    // Handle<T> has two fields: index (i64) and generation (i32)
    if (objectType->getKind() == TypeKind::HANDLE) {
        if (expr->member == "index") {
            return typeSystem->getPrimitiveType("int64");  // size_t = i64
        } else if (expr->member == "generation") {
            return typeSystem->getPrimitiveType("int32");  // u32 = i32
        } else {
            addError("Handle<T> has no member named '" + expr->member + 
                    "'. Available fields: index, generation", expr);
            return typeSystem->getErrorType();
        }
    }
    
    // For pointer types with safe navigation, check if it's a pointer to struct
    if (objectType->getKind() == TypeKind::POINTER) {
        PointerType* ptrType = static_cast<PointerType*>(objectType);
        Type* pointeeType = ptrType->getPointeeType();

        if (pointeeType->getKind() == TypeKind::STRUCT) {
            StructType* structType = static_cast<StructType*>(pointeeType);
            const auto& fields = structType->getFields();

            // Look up member in struct fields
            for (const auto& field : fields) {
                if (field.name == expr->member) {
                    return field.type;
                }
            }

            // Member not found - provide enhanced error with suggestions
            std::string bestMatch;
            size_t bestDistance = SIZE_MAX;
            size_t maxDistance = std::max(size_t(2), expr->member.length() / 2);

            std::string availableFields;
            for (size_t i = 0; i < fields.size(); ++i) {
                if (i > 0) availableFields += ", ";
                availableFields += fields[i].name;

                size_t dist = levenshteinDistance(expr->member, fields[i].name);
                if (dist < bestDistance && dist <= maxDistance) {
                    bestDistance = dist;
                    bestMatch = fields[i].name;
                }
            }

            std::string errorMsg = "Struct '" + structType->getName() + "' has no member named '" +
                    expr->member + "'";

            if (!bestMatch.empty()) {
                errorMsg += ". Did you mean '" + bestMatch + "'?";
            }

            if (!availableFields.empty()) {
                errorMsg += "\n  Available fields: " + availableFields;
            }

            addError(errorMsg, expr);
            return typeSystem->getErrorType();
        }
    }
    
    // TODO: Handle other types (obj, dyn, etc.)
    addError("Member access requires struct, object, or union type, got '" + 
            objectType->toString() + "'", expr);
    return typeSystem->getErrorType();
}

// ============================================================================
// Ternary Expression Type Inference
// ============================================================================

Type* TypeChecker::inferTernaryExpr(TernaryExpr* expr) {
    // Infer condition type
    Type* condType = inferType(expr->condition.get());
    
    if (condType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Condition must be bool
    PrimitiveType* condPrim = dynamic_cast<PrimitiveType*>(condType);
    if (!condPrim || condPrim->getName() != "bool") {
        addError("Ternary operator condition must be 'bool' type, got '" + 
                condType->toString() + "'", expr->condition.get());
        return typeSystem->getErrorType();
    }
    
    // Infer branch types
    Type* trueType = inferType(expr->trueValue.get());
    Type* falseType = inferType(expr->falseValue.get());
    
    if (trueType->getKind() == TypeKind::ERROR || falseType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Find common type for branches
    Type* resultType = findCommonType(trueType, falseType);
    
    if (resultType->getKind() == TypeKind::ERROR) {
        addError("Ternary operator branches have incompatible types: '" + 
                trueType->toString() + "' and '" + falseType->toString() + "'", expr);
    }
    
    return resultType;
}

Type* TypeChecker::inferUnwrapExpr(UnwrapExpr* expr) {
    // Infer the result expression type
    Type* resultType = inferType(expr->result.get());
    
    if (resultType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Infer the default value type
    Type* defaultType = inferType(expr->defaultValue.get());
    
    if (defaultType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Three operators use UnwrapExpr:
    // ? operator: result_expr ? default (unwraps Result<T>)
    // ?? operator: nullable_expr ?? default (null coalescing for pointers and optionals)
    // ?! operator: result_expr ?! default (emphatic unwrap for Result<T>)
    
    // Determine operator name for error messages
    std::string opName = expr->isFailsafe ? "?!" : (expr->isNullCoalesce ? "??" : "?");
    
    Type* valueType = resultType;
    
    if (resultType->getKind() == TypeKind::RESULT) {
        // ? or ?! operator: Extract T from Result<T>
        ResultType* resType = static_cast<ResultType*>(resultType);
        valueType = resType->getValueType();
        
        // Fits-or-fails for unsuffixed integer literals:
        // If the default is an unsuffixed integer literal (defaults to int32),
        // check if the value fits in the Result's value type (e.g., tbb32).
        // This mirrors how variable declarations handle `tbb32:x = 42;`.
        if (!defaultType->isAssignableTo(valueType) && !canCoerce(defaultType, valueType)) {
            bool literalRetyped = false;
            if (expr->defaultValue->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(expr->defaultValue.get());
                if (lit->explicit_type.empty() && std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = std::get<int64_t>(lit->value);
                    if (integerFitsInType(val, valueType->toString())) {
                        defaultType = valueType;
                        literalRetyped = true;
                    }
                }
            }
            if (!literalRetyped) {
                addError("Unwrap operator (" + opName + ") default value type '" + defaultType->toString() + 
                        "' does not match result value type '" + valueType->toString() + "'", expr);
                return typeSystem->getErrorType();
            }
        }
    } 
    else if (resultType->getKind() == TypeKind::POINTER) {
        // Emphatic unwrap (?!) only works on Result<T>, not pointers
        if (expr->isFailsafe) {
            addError("Emphatic unwrap operator (?!) requires Result<T> type, got '" + 
                    resultType->toString() + "'. Use ?? for null coalescing on pointers.", expr);
            return typeSystem->getErrorType();
        }
        
        // ?? operator: Extract T from ptr T
        PointerType* ptrType = static_cast<PointerType*>(resultType);
        valueType = ptrType->getPointeeType();
        
        // Check that default value type matches the pointee type
        if (!defaultType->isAssignableTo(valueType) && !canCoerce(defaultType, valueType)) {
            addError("Null coalesce operator (??) default value type '" + defaultType->toString() + 
                    "' does not match pointer target type '" + valueType->toString() + "'", expr);
            return typeSystem->getErrorType();
        }
    }
    else if (resultType->getKind() == TypeKind::OPTIONAL) {
        // Emphatic unwrap (?!) only works on Result<T>, not optionals
        if (expr->isFailsafe) {
            addError("Emphatic unwrap operator (?!) requires Result<T> type, got '" + 
                    resultType->toString() + "'. Use ?? for null coalescing on optionals.", expr);
            return typeSystem->getErrorType();
        }
        
        // ?? operator: Extract T from Optional<T> (T?)
        OptionalType* optType = static_cast<OptionalType*>(resultType);
        valueType = optType->getWrappedType();
        
        // Check that default value type matches the wrapped type
        if (!defaultType->isAssignableTo(valueType) && !canCoerce(defaultType, valueType)) {
            addError("Null coalesce operator (??) default value type '" + defaultType->toString() + 
                    "' does not match optional type '" + valueType->toString() + "'", expr);
            return typeSystem->getErrorType();
        }
    }
    else if (resultType->getKind() == TypeKind::UNKNOWN) {
        // Unknown call result (e.g. function pointer call whose return type isn't
        // tracked by the type checker yet).  Trust the default value's type.
        return defaultType;
    }
    else {
        std::string validTypes = expr->isFailsafe ? 
            "Result<T>" : 
            "Result<T>, Optional<T>, or ptr T";
        addError("Unwrap operator (" + opName + ") requires " + validTypes + " type, got '" + 
                resultType->toString() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    // All operators return the value type (T from Result<T>, Optional<T>, or ptr T)
    return valueType;
}


Type* TypeChecker::inferMoveExpr(MoveExpr* expr) {
    // Move expression transfers ownership of a variable
    // The type is the same as the variable being moved
    Type* varType = inferType(expr->variable.get());
    
    if (varType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // For now, we just return the same type
    // The borrow checker will handle tracking the move and invalidation
    return varType;
}

Type* TypeChecker::inferObjectLiteral(ObjectLiteralExpr* expr) {
    // If type_name is set, this is a struct constructor
    if (!expr->type_name.empty()) {
        // Result{val:..., err:..., is_error:...} — explicit Result construction (ToS operator)
        // Infer type from the enclosing function's return type
        if (expr->type_name == "Result") {
            if (currentFunctionReturnType) {
                return currentFunctionReturnType;
            }
            addError("Result{...} literal used outside of a function context", expr);
            return typeSystem->getErrorType();
        }
        
        // Lookup struct type
        Type* structType = typeSystem->getStructType(expr->type_name);
        
        if (!structType) {
            addError("Unknown struct type: '" + expr->type_name + "'", expr);
            return typeSystem->getErrorType();
        }
        
        // Return the struct type
        // Field validation is done in checkVarDecl for positional literals
        // or in parsePostfix for named struct literals
        return structType;
    }
    
    // Dynamic object literal (no type_name): return obj type
    Type* objType = typeSystem->getPrimitiveType("obj");
    if (!objType) {
        addError("obj type not defined in type system", expr);
        return typeSystem->getErrorType();
    }
    
    return objType;
}

// ============================================================================
// Array Literal Type Inference
// ============================================================================

Type* TypeChecker::inferArrayLiteral(ArrayLiteralExpr* expr) {
    // Empty array literal: type cannot be inferred without context
    if (expr->elements.empty()) {
        addError("Cannot infer type of empty array literal", expr);
        return typeSystem->getErrorType();
    }
    
    // Infer type from first element
    Type* elementType = inferType(expr->elements[0].get());
    if (elementType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Check all elements have compatible types
    for (size_t i = 1; i < expr->elements.size(); ++i) {
        Type* elemType = inferType(expr->elements[i].get());
        if (elemType->getKind() == TypeKind::ERROR) {
            return typeSystem->getErrorType();
        }
        
        if (!elemType->equals(elementType) && !canCoerce(elemType, elementType)) {
            addError("Array element " + std::to_string(i) + " has incompatible type: " +
                    "expected '" + elementType->toString() + "', got '" + elemType->toString() + "'", 
                    expr->elements[i].get());
            return typeSystem->getErrorType();
        }
    }
    
    // P2.7 FIX: Return proper array type with size, not pointer type
    // Array literals should have known size at compile time
    size_t arraySize = expr->elements.size();
    return typeSystem->getArrayType(elementType, arraySize);
}

// ============================================================================
// Range Expression Type Inference
// ============================================================================

Type* TypeChecker::inferRangeExpr(RangeExpr* expr) {
    // Check start expression
    Type* startType = inferType(expr->start.get());
    if (startType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Check end expression
    Type* endType = inferType(expr->end.get());
    if (endType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Verify both are integer types (ranges only work with integers)
    PrimitiveType* startPrim = dynamic_cast<PrimitiveType*>(startType);
    PrimitiveType* endPrim = dynamic_cast<PrimitiveType*>(endType);
    
    if (!startPrim || !endPrim) {
        addError("Range expressions require integer types, got '" + 
                startType->toString() + "' and '" + endType->toString() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    // Check if both are integer types
    bool startIsInt = (startPrim->getName().find("int") != std::string::npos || 
                       startPrim->getName().find("tbb") != std::string::npos ||
                       startPrim->getName().find("bal") != std::string::npos);
    bool endIsInt = (endPrim->getName().find("int") != std::string::npos ||
                     endPrim->getName().find("tbb") != std::string::npos ||
                     endPrim->getName().find("bal") != std::string::npos);
    
    if (!startIsInt || !endIsInt) {
        addError("Range expressions require integer types, got '" + 
                startType->toString() + "' and '" + endType->toString() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    // Range type is opaque - just return int64 as a placeholder
    // The actual range is a struct {int64, int64, bool} created during codegen
    return typeSystem->getPrimitiveType("int64");
}

// ============================================================================
// Type Compatibility and Coercion
// ============================================================================

Type* TypeChecker::findCommonType(Type* left, Type* right) {
    // If types are equal, return either one
    if (left->equals(right)) {
        return left;
    }
    
    // Try coercion in both directions
    if (canCoerce(left, right)) {
        return right;  // Widen left to right
    }
    
    if (canCoerce(right, left)) {
        return left;  // Widen right to left
    }
    
    // No common type found
    addError("No common type between '" + left->toString() + 
            "' and '" + right->toString() + "'", 0, 0);
    return typeSystem->getErrorType();
}

bool TypeChecker::canCoerce(Type* from, Type* to) {
    // Same type: always coercible
    if (from->equals(to)) {
        return true;
    }
    
    // any type accepts assignment from any other type (type erasure)
    // This is the entry point for type-erased storage.
    // Getting the value back out requires resolve<T>() or get<T>().
    if (to->getKind() == TypeKind::ANY) {
        return true;
    }
    
    // Only handle primitive type coercion for now
    PrimitiveType* fromPrim = dynamic_cast<PrimitiveType*>(from);
    PrimitiveType* toPrim = dynamic_cast<PrimitiveType*>(to);
    
    if (!fromPrim || !toPrim) {
        return false;  // Non-primitive types require exact match
    }
    
    const std::string& fromName = fromPrim->getName();
    const std::string& toName = toPrim->getName();
    
    // ========================================================================
    // Aria STRICT Type Coercion Rules
    // ========================================================================
    // - NO implicit integer widening (int32 → int64 requires explicit cast)
    // - NO cross-family coercion (int ↔ tbb ↔ balanced FORBIDDEN)
    // - NO implicit TBB widening (ARIA-018: Sentinel Discontinuity Constraint)
    //   tbb8 ERR (-128) → tbb16 becomes valid -128, not ERR (-32768)
    //   Use explicit tbb_widen<T>() for safe sentinel-preserving conversion
    // - Only float → float widening allowed
    // - Only balanced → same-subfamily balanced widening allowed
    // ========================================================================

    // Extract bit width from type name (e.g., "int32" → 32)
    // Returns 0 for invalid or malformed type names (overflow, non-numeric, etc.)
    auto extractBitWidth = [](const std::string& name) -> int {
        try {
            std::string suffix;
            if (name.find("int") == 0 || name.find("uint") == 0) {
                size_t pos = (name[0] == 'u') ? 4 : 3;  // "uint" or "int"
                suffix = name.substr(pos);
            } else if (name.find("flt") == 0) {
                suffix = name.substr(3);
            } else if (name.find("tbb") == 0) {
                suffix = name.substr(3);
            } else {
                return 0;
            }

            // Use stoll to handle larger values, then validate range
            long long width = std::stoll(suffix);

            // Valid bit widths for Aria types
            if (width == 8 || width == 16 || width == 32 || width == 64 ||
                width == 128 || width == 256 || width == 512 || width == 1024 || width == 2048 || width == 4096) {
                return static_cast<int>(width);
            }
            return 0;  // Invalid bit width
        } catch (const std::out_of_range&) {
            return 0;  // Overflow - not a valid bit width
        } catch (const std::invalid_argument&) {
            return 0;  // Not a valid number
        }
    };

    // Check type families
    bool fromIsTBB = (fromName.find("tbb") == 0);
    bool toIsTBB = (toName.find("tbb") == 0);
    bool fromIsBalanced = (fromName == "trit" || fromName == "tryte" ||
                          fromName == "nit" || fromName == "nyte");
    bool toIsBalanced = (toName == "trit" || toName == "tryte" ||
                        toName == "nit" || toName == "nyte");
    bool fromIsFloat = (fromName.find("flt") == 0);
    bool toIsFloat = (toName.find("flt") == 0);
    bool fromIsInt = ((fromName.find("int") == 0 || fromName.find("uint") == 0) && !fromIsTBB);
    bool toIsInt = ((toName.find("int") == 0 || toName.find("uint") == 0) && !toIsTBB);

    // ========================================================================
    // TBB Type Family - NO implicit coercion (even between TBB sizes)
    // ========================================================================
    // ARIA-018: TBB sizes have different sentinel values:
    //   tbb8 sentinel = -128, tbb16 sentinel = -32768, etc.
    // Widening -128 from tbb8 to tbb16 would create a valid value,
    // not a sentinel, breaking sticky error semantics.
    // Use explicit tbb_widen<T>() functions for size conversion.
    if (toIsTBB) {
        if (!fromIsTBB) {
            return false;  // REJECT: int → tbb, balanced → tbb, float → tbb
        }
        // REJECT: tbb8 → tbb16, etc. (different sentinel values)
        // Only allow exact same type
        return fromName == toName;
    }
    if (fromIsTBB) {
        return false;  // REJECT: tbb → anything else
    }

    // ========================================================================
    // Balanced Type Family - ONLY same-subfamily widening
    // ========================================================================
    if (toIsBalanced) {
        if (!fromIsBalanced) {
            return false;  // REJECT: int → balanced, float → balanced
        }
        // Only same sub-family widening: trit → tryte, nit → nyte
        bool fromIsTernary = (fromName == "trit" || fromName == "tryte");
        bool toIsTernary = (toName == "trit" || toName == "tryte");
        if (fromIsTernary != toIsTernary) {
            return false;  // REJECT: ternary ↔ nonary
        }
        // Allow widening within same sub-family
        // trit (2 bits) → tryte (16 bits), nit (4 bits) → nyte (16 bits)
        if (fromName == "trit" && toName == "tryte") return true;
        if (fromName == "nit" && toName == "nyte") return true;
        return false;  // No other balanced widening
    }
    if (fromIsBalanced) {
        return false;  // REJECT: balanced → int, balanced → float
    }

    // ========================================================================
    // Float Widening - ONLY float → float widening allowed
    // ========================================================================
    if (fromIsFloat && toIsFloat) {
        int fromWidth = extractBitWidth(fromName);
        int toWidth = extractBitWidth(toName);
        return fromWidth < toWidth;  // Allow widening only
    }

    // ========================================================================
    // fix256 literal initialization from int or float
    // ========================================================================
    // fix256:x = 1.0 or fix256:x = 42 auto-converts via runtime
    if (toName == "fix256" && (fromIsFloat || fromIsInt)) {
        return true;
    }

    // REJECT: int → float (requires explicit cast)
    if (fromIsInt && toIsFloat) {
        return false;
    }

    // REJECT: float → int
    if (fromIsFloat && toIsInt) {
        return false;
    }

    // ========================================================================
    // Integer Widening - ALLOWED within same family
    // ========================================================================
    // int8 → int16 → int32 → int64 OK (signed family)
    // uint8 → uint16 → uint32 → uint64 OK (unsigned family)
    // int ↔ uint REJECTED (sign change)

    bool fromIsSigned = (fromName.find("int") == 0 && fromName.find("uint") == std::string::npos);
    bool toIsSigned = (toName.find("int") == 0 && toName.find("uint") == std::string::npos);
    bool fromIsUnsigned = (fromName.find("uint") == 0);
    bool toIsUnsigned = (toName.find("uint") == 0);

    // Signed integer widening
    if (fromIsSigned && toIsSigned) {
        int fromWidth = extractBitWidth(fromName);
        int toWidth = extractBitWidth(toName);
        return fromWidth < toWidth;  // Allow widening only
    }

    // Unsigned integer widening
    if (fromIsUnsigned && toIsUnsigned) {
        int fromWidth = extractBitWidth(fromName);
        int toWidth = extractBitWidth(toName);
        return fromWidth < toWidth;  // Allow widening only
    }

    // REJECT: signed ↔ unsigned (int32 ↔ uint32)
    return false;
}

// ============================================================================
// FFI Pointer Conversion (P1.5 - void* ↔ wild T-> Bridge)
// ============================================================================

bool TypeChecker::canConvertFFIPointer(Type* from, Type* to) {
    // Check if one type is void* (C FFI) and the other is a wild pointer (Aria)
    
    std::string fromStr = from->toString();
    std::string toStr = to->toString();
    
    // void* → wild T-> conversion (from extern function return value)
    // Example: wild int32->:ptr = malloc(400);
    if (fromStr == "void*" && to->getKind() == TypeKind::POINTER) {
        return true;  // Allow automatic conversion from void* to any wild pointer
    }
    
    // wild T-> → void* conversion (to extern function parameter)
    // Example: free(ptr) where ptr is wild int32->
    if (from->getKind() == TypeKind::POINTER && toStr == "void*") {
        return true;  // Allow automatic conversion from any wild pointer to void*
    }
    
    return false;
}

// ============================================================================
// Error Handling
// ============================================================================

void TypeChecker::addError(const std::string& message, int line, int column) {
    std::ostringstream oss;
    if (line > 0) {
        oss << "Line " << line << ", Column " << column << ": ";
    }
    oss << message;
    errors.push_back(oss.str());
}

void TypeChecker::addError(const std::string& message, ASTNode* node) {
    if (node) {
        addError(message, node->line, node->column);
    } else {
        addError(message, 0, 0);
    }
}

// ============================================================================
// Statement Type Checking - Phase 3.2.3
// ============================================================================

void TypeChecker::checkStatement(ASTNode* stmt) {
    if (!stmt) {
        return;
    }
    
    switch (stmt->type) {
        case ASTNode::NodeType::VAR_DECL:
            checkVarDecl(static_cast<VarDeclStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::FUNC_DECL:
            checkFuncDecl(static_cast<FuncDeclStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::STRUCT_DECL:
            checkStructDecl(static_cast<StructDeclStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::ENUM_DECL:
            checkEnumDecl(static_cast<EnumDeclStmt*>(stmt));
            break;

        case ASTNode::NodeType::RULES_DECL:
            checkRulesDecl(static_cast<RulesDeclStmt*>(stmt));
            break;

        case ASTNode::NodeType::TYPE_DECL:
            checkTypeDecl(static_cast<TypeDeclStmt*>(stmt));
            break;

        case ASTNode::NodeType::TRAIT_DECL:
            checkTraitDecl(static_cast<TraitDeclStmt*>(stmt));
            break;

        case ASTNode::NodeType::IMPL_DECL:
            checkImplDecl(static_cast<ImplDeclStmt*>(stmt));
            break;

        case ASTNode::NodeType::RETURN:
            checkReturnStmt(static_cast<ReturnStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::PASS:
            checkPassStmt(static_cast<PassStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::FAIL:
            checkFailStmt(static_cast<FailStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::IF:
            checkIfStmt(static_cast<IfStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::WHILE:
            checkWhileStmt(static_cast<WhileStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::FOR:
            checkForStmt(static_cast<ForStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::TILL:
            checkTillStmt(static_cast<TillStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::LOOP:
            checkLoopStmt(static_cast<LoopStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::WHEN:
            checkWhenStmt(static_cast<WhenStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::BLOCK:
            checkBlockStmt(static_cast<BlockStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::EXPRESSION_STMT:
            checkExpressionStmt(static_cast<ExpressionStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::BREAK:
        case ASTNode::NodeType::CONTINUE:
            // No type checking needed for break/continue
            break;
        
        case ASTNode::NodeType::USE:
            checkUseStmt(static_cast<UseStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::MOD:
            checkModStmt(static_cast<ModStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::PICK:
            checkPickStmt(static_cast<PickStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::EXTERN:
            checkExternStmt(static_cast<ExternStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::COMPTIME_BLOCK:
            checkComptimeBlock(static_cast<ComptimeBlockStmt*>(stmt));
            break;
        
        default:
            // Other statement types not yet implemented
            break;
    }
}

// ============================================================================
// Type Resolution from AST Nodes
// ============================================================================

Type* TypeChecker::resolveTypeNode(ASTNode* typeNode) {
    if (!typeNode) {
        return typeSystem->getErrorType();
    }
    
    switch (typeNode->type) {
        case ASTNode::NodeType::TYPE_ANNOTATION: {
            // Simple type: int32, string, etc.
            aria::SimpleType* simpleType = static_cast<aria::SimpleType*>(typeNode);
            
            // Handle vector types specially (vec2, vec3)
            // Default to flt64 components to match float literal inference
            if (simpleType->typeName == "vec2" || simpleType->typeName == "dvec2") {
                Type* componentType = typeSystem->getPrimitiveType("flt64");
                return typeSystem->getVectorType(componentType, 2);
            }
            if (simpleType->typeName == "vec3" || simpleType->typeName == "dvec3") {
                Type* componentType = typeSystem->getPrimitiveType("flt64");
                return typeSystem->getVectorType(componentType, 3);
            }
            // dvec9: Double-precision floating-point vector (9 flt64 components)
            if (simpleType->typeName == "dvec9") {
                Type* componentType = typeSystem->getPrimitiveType("flt64");
                return typeSystem->getVectorType(componentType, 9);
            }
            // vec9: TBB-inspired vector (vec9_tbb32 struct - NOT a VectorType)
            // Handled below via struct/primitive lookup
            
            // Float vector types (fvec2, fvec3, fvec9)
            if (simpleType->typeName == "fvec2") {
                Type* componentType = typeSystem->getPrimitiveType("flt32");
                return typeSystem->getVectorType(componentType, 2);
            }
            if (simpleType->typeName == "fvec3") {
                Type* componentType = typeSystem->getPrimitiveType("flt32");
                return typeSystem->getVectorType(componentType, 3);
            }
            if (simpleType->typeName == "fvec9") {
                Type* componentType = typeSystem->getPrimitiveType("flt32");
                return typeSystem->getVectorType(componentType, 9);
            }
            
            // Integer vector types (ivec2, ivec3, ivec9)
            if (simpleType->typeName == "ivec2") {
                Type* componentType = typeSystem->getPrimitiveType("int32");
                return typeSystem->getVectorType(componentType, 2);
            }
            if (simpleType->typeName == "ivec3") {
                Type* componentType = typeSystem->getPrimitiveType("int32");
                return typeSystem->getVectorType(componentType, 3);
            }
            if (simpleType->typeName == "ivec9") {
                Type* componentType = typeSystem->getPrimitiveType("int32");
                return typeSystem->getVectorType(componentType, 9);
            }
            
            // Try struct first
            Type* type = typeSystem->getStructType(simpleType->typeName);
            if (type) {
                return type;
            }
            
            // v0.2.39: Try enum type
            type = typeSystem->getEnumType(simpleType->typeName);
            if (type) {
                return type;
            }
            
            // any type — type-erased pointer (safe void* replacement)
            if (simpleType->typeName == "any") {
                return typeSystem->getAnyType();
            }

            // v0.2.36: dyn Trait types — fat pointer for dynamic dispatch
            if (simpleType->typeName.substr(0, 4) == "dyn ") {
                std::string traitName = simpleType->typeName.substr(4);
                return typeSystem->getDynTraitType(traitName);
            }
            
            // Try primitive type
            type = typeSystem->getPrimitiveType(simpleType->typeName);
            if (type && type->getKind() != TypeKind::ERROR) {
                return type;
            }
            
            {
                std::string errorMsg = "Unknown type: '" + simpleType->typeName + "'";
                std::string suggestion = findSimilarType(simpleType->typeName);
                if (!suggestion.empty()) {
                    errorMsg += ". Did you mean '" + suggestion + "'?";
                    // Add extra hint for common casing mistakes
                    std::string lower = simpleType->typeName;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    if (lower == suggestion || lower == "nil") {
                        errorMsg += " (Aria types are lowercase)";
                    }
                }
                addError(errorMsg, typeNode);
            }
            return typeSystem->getErrorType();
        }
        
        case ASTNode::NodeType::ARRAY_TYPE: {
            // Array type: int32[], int32[10], etc.
            aria::ArrayType* arrayType = static_cast<aria::ArrayType*>(typeNode);
            
            // Resolve element type
            Type* elementType = resolveTypeNode(arrayType->elementType.get());
            if (elementType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Determine array size
            int arraySize = -1;  // -1 indicates dynamic/unknown size (int32[])
            
            if (!arrayType->isDynamic && arrayType->sizeExpr) {
                // Fixed-size array (int32[10])
                // Need to evaluate constant expression for size
                // For now, if it's a literal, extract the value
                if (arrayType->sizeExpr->type == ASTNode::NodeType::LITERAL) {
                    LiteralExpr* sizeLit = static_cast<LiteralExpr*>(arrayType->sizeExpr.get());
                    
                    // Extract integer from variant
                    if (std::holds_alternative<int64_t>(sizeLit->value)) {
                        int64_t size64 = std::get<int64_t>(sizeLit->value);
                        if (size64 < 0) {
                            addError("Array size must be non-negative, got " + std::to_string(size64), typeNode);
                            return typeSystem->getErrorType();
                        }
                        arraySize = static_cast<int>(size64);
                    } else {
                        addError("Array size must be an integer literal", typeNode);
                        return typeSystem->getErrorType();
                    }
                } else {
                    // TODO: Evaluate constant expression for size
                    // For now, treat as dynamic if not a simple literal
                    addError("Array size must be a constant integer literal (for now)", typeNode);
                    return typeSystem->getErrorType();
                }
            }
            
            // Create proper array type
            return typeSystem->getArrayType(elementType, arraySize);
        }
        
        case ASTNode::NodeType::POINTER_TYPE: {
            // Pointer type: int32@, string@, ?@ (erased), etc.
            aria::PointerType* ptrType = static_cast<aria::PointerType*>(typeNode);
            
            // ARIA-P3: ?-> / ?* — type-erased pointer
            // baseType is null; create an erased sema PointerType directly.
            if (ptrType->isErased) {
                return typeSystem->getErasedPointerType();
            }
            
            // Resolve base type
            Type* baseType = resolveTypeNode(ptrType->baseType.get());
            if (baseType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            return typeSystem->getPointerType(baseType);
        }
        
        case ASTNode::NodeType::SAFE_REF_TYPE: {
            // Safe reference type: int32$, string$, etc.
            // For now, treat safe references as pointer types in the type system
            // The borrow checker will enforce the safety rules
            aria::SafeRefType* safeRefType = static_cast<aria::SafeRefType*>(typeNode);
            
            // Resolve base type
            Type* baseType = resolveTypeNode(safeRefType->baseType.get());
            if (baseType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            // Use pointer type internally, borrow checker handles safety
            return typeSystem->getPointerType(baseType);
        }
        
        case ASTNode::NodeType::OPTIONAL_TYPE: {
            // Optional type: int64?, string?, etc.
            aria::OptionalTypeNode* optType = static_cast<aria::OptionalTypeNode*>(typeNode);
            
            // Resolve wrapped type
            Type* wrappedType = resolveTypeNode(optType->wrappedType.get());
            if (wrappedType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            
            return typeSystem->getOptionalType(wrappedType);
        }
        
        case ASTNode::NodeType::GENERIC_TYPE: {
            // Generic type instantiation: Box<int64>, Result<int32, string>, etc.
            aria::GenericType* genericType = static_cast<aria::GenericType*>(typeNode);
            
            // Phase 3.4: Generic Struct Instantiation (Session 13)
            // Resolve all type arguments
            std::vector<Type*> resolvedTypeArgs;
            TypeSubstitution substitution;
            
            for (const auto& typeArg : genericType->typeArgs) {
                Type* argType = resolveTypeNode(typeArg.get());
                if (argType->getKind() == TypeKind::ERROR) {
                    return typeSystem->getErrorType();
                }
                resolvedTypeArgs.push_back(argType);
            }
            
            // Check for built-in generic types first
            if (genericType->baseName == "Result") {
                // Result<T> is a built-in type, not a struct
                if (resolvedTypeArgs.size() != 1) {
                    addError("Result<T> requires exactly 1 type argument, got " + 
                            std::to_string(resolvedTypeArgs.size()), typeNode);
                    return typeSystem->getErrorType();
                }
                
                // Use the built-in ResultType
                return typeSystem->getResultType(resolvedTypeArgs[0]);
            }
            
            // P1-3: Handle<T> - Generational memory handles for safe neurogenesis
            if (genericType->baseName == "Handle") {
                // Handle<T> is a built-in type for arena-based memory management
                if (resolvedTypeArgs.size() != 1) {
                    addError("Handle<T> requires exactly 1 type argument, got " + 
                            std::to_string(resolvedTypeArgs.size()), typeNode);
                    return typeSystem->getErrorType();
                }
                
                // Create HandleType wrapping the pointee type
                return typeSystem->getHandleType(resolvedTypeArgs[0]);
            }
            
            // P1-2: simd<T, N> - SIMD vectorization for performance
            if (genericType->baseName == "simd") {
                // simd<T, N> requires exactly 2 type arguments:
                // - T: element type (must be primitive for now)
                // - N: lane count (must be compile-time constant integer)
                if (genericType->typeArgs.size() != 2) {
                    addError("simd<T, N> requires exactly 2 type arguments, got " + 
                            std::to_string(genericType->typeArgs.size()), typeNode);
                    return typeSystem->getErrorType();
                }
                
                // First argument: element type
                Type* elementType = resolvedTypeArgs[0];
                
                // Validate element type is primitive (for now)
                if (!elementType->isPrimitive()) {
                    addError("simd<T, N> element type must be primitive, got: " + 
                            elementType->toString(), typeNode);
                    return typeSystem->getErrorType();
                }
                
                // Second argument: lane count (must be a simple type name representing an integer)
                // Support two formats:
                // 1. Plain integer: "4", "8", "16" (Phase 2: parser handles this)
                // 2. Identifier format: "N4", "N8", "N16" (Phase 1 workaround, still supported)
                aria::ASTNode* laneCountNode = genericType->typeArgs[1].get();
                size_t laneCount = 0;
                
                if (laneCountNode->type == ASTNode::NodeType::TYPE_ANNOTATION) {
                    aria::SimpleType* laneCountType = static_cast<aria::SimpleType*>(laneCountNode);
                    std::string laneCountStr = laneCountType->typeName;
                    
                    // Check for "N<number>" format (e.g., "N4", "N8", "N16") - Phase 1 workaround
                    if (laneCountStr.length() > 1 && laneCountStr[0] == 'N') {
                        laneCountStr = laneCountStr.substr(1);  // Strip 'N' prefix
                    }
                    // Otherwise, it should be a plain integer from the parser (Phase 2)
                    
                    try {
                        laneCount = std::stoull(laneCountStr);
                    } catch (...) {
                        addError("simd<T, N> lane count must be a positive integer (e.g., 4, 8, 16 or N4, N8, N16), got: " + 
                                laneCountType->typeName, typeNode);
                        return typeSystem->getErrorType();
                    }
                } else {
                    addError("simd<T, N> lane count must be a compile-time constant", typeNode);
                    return typeSystem->getErrorType();
                }
                
                // Validate lane count is valid (power of 2, reasonable range)
                if (laneCount == 0 || laneCount > 64) {
                    addError("simd<T, N> lane count must be between 1 and 64, got: " + 
                            std::to_string(laneCount), typeNode);
                    return typeSystem->getErrorType();
                }
                
                // TODO: Validate lane count is power of 2 (for now, allow any value)
                
                // Create SimdType
                return typeSystem->getSimdType(elementType, laneCount);
            }
            
            if (genericType->baseName == "array" || genericType->baseName == "atomic" || 
                genericType->baseName == "Vec" || genericType->baseName == "HashMap") {
                // Built-in generic types - handle specially
                // For now, just return a placeholder struct type
                // The actual implementation will be in stdlib once fully integrated
                
                // atomic<T> requires special validation
                if (genericType->baseName == "atomic") {
                    // Validate argument count
                    if (resolvedTypeArgs.size() != 1) {
                        addError("atomic<T> requires exactly 1 type argument, got " + 
                                std::to_string(resolvedTypeArgs.size()), typeNode);
                        return typeSystem->getErrorType();
                    }
                    
                    // Validate type compatibility
                    Type* argType = resolvedTypeArgs[0];
                    if (!isAtomicCompatible(argType)) {
                        std::ostringstream msg;
                        msg << "atomic<T> requires lock-free compatible type (int8-64, uint8-64, bool, tbb8-64), got: "
                            << argType->toString();
                        addError(msg.str(), typeNode);
                        return typeSystem->getErrorType();
                    }
                    
                    // Create mangled name for atomic specialization
                    std::string mangledName = "atomic_" + argType->toString();
                    
                    // Check if we already have this specialization
                    Type* existingType = typeSystem->getStructType(mangledName);
                    if (existingType) {
                        return existingType;
                    }
                    
                    // Create atomic<T> struct type
                    // atomic<T> has: T value (aligned appropriately for atomic operations)
                    std::vector<StructType::Field> fields;
                    fields.push_back({"value", argType, 0});
                    
                    return typeSystem->createStructType(mangledName, fields);
                }
                
                // Vec<T> - Dynamic array (P0 feature for Nikola 0.1.0)
                // Maps to runtime AriaArray struct (see src/runtime/collections/collections.cpp)
                if (genericType->baseName == "Vec") {
                    // Validate argument count
                    if (resolvedTypeArgs.size() != 1) {
                        addError("Vec<T> requires exactly 1 type argument, got " + 
                                std::to_string(resolvedTypeArgs.size()), typeNode);
                        return typeSystem->getErrorType();
                    }
                    
                    Type* elemType = resolvedTypeArgs[0];
                    
                    // Create mangled name for Vec specialization
                    std::string mangledName = "Vec_" + elemType->toString();
                    
                    // Check if we already have this specialization
                    Type* existingType = typeSystem->getStructType(mangledName);
                    if (existingType) {
                        return existingType;
                    }
                    
                    // Create Vec<T> struct type matching AriaArray layout:
                    // typedef struct {
                    //     void* data;          // Element array (stored as wild int8*)
                    //     size_t length;       // Current count
                    //     size_t capacity;     // Allocated capacity
                    //     size_t element_size; // Bytes per element
                    //     int type_id;         // GC type tracking
                    // } AriaArray;
                    std::vector<StructType::Field> fields;
                    // Use wild int8* for data (generic pointer, matches C void*)
                    fields.push_back({"data", typeSystem->getPointerType(typeSystem->getPrimitiveType("int8")), 0});
                    fields.push_back({"length", typeSystem->getPrimitiveType("int64"), 8});
                    fields.push_back({"capacity", typeSystem->getPrimitiveType("int64"), 16});
                    fields.push_back({"element_size", typeSystem->getPrimitiveType("int64"), 24});
                    fields.push_back({"type_id", typeSystem->getPrimitiveType("int32"), 32});
                    
                    return typeSystem->createStructType(mangledName, fields);
                }
                
                // HashMap<K,V> - Hash table (P0 feature for Nikola 0.1.0)
                // Maps to runtime AriaMap struct (see src/runtime/collections/map.cpp)
                if (genericType->baseName == "HashMap") {
                    // Validate argument count - HashMap requires TWO type arguments (key, value)
                    if (resolvedTypeArgs.size() != 2) {
                        addError("HashMap<K,V> requires exactly 2 type arguments, got " + 
                                std::to_string(resolvedTypeArgs.size()), typeNode);
                        return typeSystem->getErrorType();
                    }
                    
                    Type* keyType = resolvedTypeArgs[0];
                    Type* valueType = resolvedTypeArgs[1];
                    
                    // Create mangled name for HashMap specialization
                    std::string mangledName = "HashMap_" + keyType->toString() + "_" + valueType->toString();
                    
                    // Check if we already have this specialization
                    Type* existingType = typeSystem->getStructType(mangledName);
                    if (existingType) {
                        return existingType;
                    }
                    
                    // Create HashMap<K,V> struct type matching AriaMap layout:
                    // typedef struct {
                    //     AriaMapEntry* entries;  // Array of entries (stored as wild int8*)
                    //     size_t capacity;        // Total slots (power of 2)
                    //     size_t length;          // Number of occupied entries
                    //     size_t key_size;        // Size of keys in bytes
                    //     size_t value_size;      // Size of values in bytes
                    //     float load_factor;      // Max load before resize
                    // } AriaMap;
                    std::vector<StructType::Field> fields;
                    // Use wild int8* for entries (generic pointer, matches C void*)
                    fields.push_back({"entries", typeSystem->getPointerType(typeSystem->getPrimitiveType("int8")), 0});
                    fields.push_back({"capacity", typeSystem->getPrimitiveType("int64"), 8});
                    fields.push_back({"length", typeSystem->getPrimitiveType("int64"), 16});
                    fields.push_back({"key_size", typeSystem->getPrimitiveType("int64"), 24});
                    fields.push_back({"value_size", typeSystem->getPrimitiveType("int64"), 32});
                    fields.push_back({"load_factor", typeSystem->getPrimitiveType("flt32"), 40});
                    
                    return typeSystem->createStructType(mangledName, fields);
                }
                
                // Create a mangled name for this instantiation
                std::string mangledName = genericType->baseName;
                for (const auto& argType : resolvedTypeArgs) {
                    mangledName += "_" + argType->toString();
                }
                
                // Check if we already have this specialization
                Type* existingType = typeSystem->getStructType(mangledName);
                if (existingType) {
                    return existingType;
                }
                
                // Create a new struct type for this specialization
                // This is a placeholder - actual fields will be defined in stdlib
                std::vector<StructType::Field> fields;
                if (genericType->baseName == "array") {
                    // array<T> has: T* data, int64 length, int64 capacity
                    Type* elemType = resolvedTypeArgs[0];
                    Type* ptrType = typeSystem->getPointerType(elemType);
                    fields.push_back({"data", ptrType, 0});
                    fields.push_back({"length", typeSystem->getPrimitiveType("int64"), 8});
                    fields.push_back({"capacity", typeSystem->getPrimitiveType("int64"), 16});
                }
                
                return typeSystem->createStructType(mangledName, fields);
            }
            
            // P1-5 Phase 3: Dimensional Type Support
            // Check for dimensional types: fix256<Joules>, fix128<Meters>, etc.
            // Pattern: BaseType<DimensionName> where:
            // - BaseType is a numeric type (fix256, fix128, int64, etc.)
            // - DimensionName is a registered dimension (Joules, Meters, etc.)
            if (genericType->typeArgs.size() == 1) {
                // Check if the type argument is a simple identifier (potential dimension name)
                aria::ASTNode* typeArgNode = genericType->typeArgs[0].get();
                if (typeArgNode->type == ASTNode::NodeType::TYPE_ANNOTATION) {
                    aria::SimpleType* simpleTypeArg = static_cast<aria::SimpleType*>(typeArgNode);
                    std::string potentialDimensionName = simpleTypeArg->typeName;
                    
                    // Check if this is a registered dimension
                    const Dimension* dim = typeSystem->lookupDimension(potentialDimensionName);
                    if (dim != nullptr) {
                        // This is a dimensional type! Get the base numeric type
                        // Try primitive types first (int64, flt64, etc.)
                        Type* baseType = typeSystem->getPrimitiveType(genericType->baseName);
                        
                        // If not primitive, try struct types (fix256, etc.)
                        if (!baseType) {
                            baseType = typeSystem->getStructType(genericType->baseName);
                        }
                        
                        if (!baseType) {
                            addError("Unknown base type for dimensional: '" + 
                                   genericType->baseName + "' in " + 
                                   genericType->baseName + "<" + potentialDimensionName + ">", 
                                   typeNode);
                            return typeSystem->getErrorType();
                        }
                        
                        // Validate that base type is numeric
                        if (!isNumericType(baseType)) {
                            addError("Dimensional types require numeric base type, got: " + 
                                   baseType->toString(), typeNode);
                            return typeSystem->getErrorType();
                        }
                        
                        // Create and return dimensional type
                        return typeSystem->getDimensionalType(baseType, *dim, potentialDimensionName);
                    }
                }
            }
            
            // Look up the generic struct declaration
            auto it = genericStructRegistry.find(genericType->baseName);
            if (it == genericStructRegistry.end()) {
                // Not a generic struct - check if it's a regular struct
                Type* baseStructType = typeSystem->getStructType(genericType->baseName);
                if (baseStructType) {
                    addError("'" + genericType->baseName + "' is not a generic type", typeNode);
                } else {
                    addError("Unknown generic type: '" + genericType->baseName + "'", typeNode);
                }
                return typeSystem->getErrorType();
            }
            
            StructDeclStmt* genericStructDecl = it->second;
            
            // Validate type argument count
            if (resolvedTypeArgs.size() != genericStructDecl->genericParams.size()) {
                std::ostringstream msg;
                msg << "Generic type '" << genericType->baseName << "' expects "
                    << genericStructDecl->genericParams.size() << " type arguments, got "
                    << resolvedTypeArgs.size();
                addError(msg.str(), typeNode);
                return typeSystem->getErrorType();
            }
            
            // Build type substitution map
            for (size_t i = 0; i < genericStructDecl->genericParams.size(); i++) {
                substitution[genericStructDecl->genericParams[i].name] = resolvedTypeArgs[i];
            }
            
            // Check if monomorphizer is available
            if (!monomorphizer) {
                addError("Monomorphizer not initialized for generic type instantiation", typeNode);
                return typeSystem->getErrorType();
            }
            
            // Request monomorphization
            std::string mangledName = monomorphizer->requestStructSpecialization(
                genericStructDecl, substitution);
            
            if (mangledName.empty()) {
                // Error occurred during monomorphization
                if (monomorphizer->hasErrors()) {
                    for (const auto& err : monomorphizer->getErrors()) {
                        addError(err.message, err.line, err.column);
                    }
                }
                return typeSystem->getErrorType();
            }
            
            // Look up or create the specialized struct type
            Type* specializedType = typeSystem->getStructType(mangledName);
            if (!specializedType) {
                // Need to create the specialized struct type in TypeSystem
                // Get the specialized struct declaration from monomorphizer
                // For now, create a placeholder
                // TODO: Register specialized struct properly
                addError("Specialized struct type registration not yet implemented: '" + 
                        mangledName + "'", typeNode);
                return typeSystem->getErrorType();
            }
            
            return specializedType;
        }
        
        case ASTNode::NodeType::FUNCTION_TYPE: {
            // Function pointer type: (retType)(paramTypes...)
            // Return a placeholder primitive type so the type checker doesn't error.
            // The IR generator handles FUNCTION_TYPE specially via typeNode check
            // before stmt->typeName is used, so this placeholder is never lowered.
            Type* placeholder = typeSystem->getPrimitiveType("uint64");
            if (!placeholder) placeholder = typeSystem->getPrimitiveType("int64");
            return placeholder ? placeholder : typeSystem->getErrorType();
        }

        default:
            addError("Unsupported type node in type resolution", typeNode);
            return typeSystem->getErrorType();
    }
}

// ============================================================================
// Variable Declaration Type Checking
// ============================================================================

void TypeChecker::checkVarDecl(VarDeclStmt* stmt) {
    // Prevent shadowing reserved builtin names
    static const std::unordered_set<std::string> reservedBuiltins = {
        "ok", "print", "println", "stdout_write", "stderr_write",
        "to_string", "fail", "pass", "drop", "raw", "sleep_ms", "exit", "env_get", "sort_lines",
        "sys"
    };
    if (reservedBuiltins.count(stmt->varName)) {
        addError("'" + stmt->varName + "' is a reserved builtin and cannot be used as a variable name", stmt);
        return;
    }

    // ========================================================================
    // Borrow Qualifier Validation (v0.2.35)
    // $$i = immutable borrow, $$m = mutable borrow
    // ========================================================================
    if (stmt->isBorrowImm || stmt->isBorrowMut) {
        // Cannot have both $$i and $$m
        if (stmt->isBorrowImm && stmt->isBorrowMut) {
            addError("Cannot use both $$i (immutable borrow) and $$m (mutable borrow) on the same variable", stmt);
            return;
        }
        // Cannot combine borrow with wild/wildx
        if (stmt->isWild || stmt->isWildx) {
            addError("Borrow qualifiers ($$i/$$m) cannot be combined with wild/wildx — "
                     "borrows reference existing memory, they don't allocate", stmt);
            return;
        }
        // Borrow must have an initializer (what are we borrowing?)
        if (!stmt->initializer) {
            addError("Borrow variable '" + stmt->varName + "' must have an initializer — "
                     "$$i/$$m must reference an existing variable", stmt);
            return;
        }
        // Borrow initializer must be a variable reference or struct field access
        if (stmt->initializer->type != ASTNode::NodeType::IDENTIFIER &&
            stmt->initializer->type != ASTNode::NodeType::MEMBER_ACCESS) {
            addError("Borrow variable '" + stmt->varName + "' must borrow from a named variable, "
                     "not a literal or expression", stmt);
            return;
        }
    }

    Type* declaredType = nullptr;
    
    // Handle new typeNode or legacy typeName
    if (stmt->typeNode) {
        // Resolve type from AST node (supports arrays, pointers, etc.)
        declaredType = resolveTypeNode(stmt->typeNode.get());
        if (!declaredType || declaredType->getKind() == TypeKind::ERROR) {
            addError("Cannot resolve type in variable declaration", stmt);
            return;
        }
        
        // Array size inference: int32[]:x = [1, 2, 3] → narrow to int32[3]
        if (declaredType->getKind() == TypeKind::ARRAY) {
            const ArrayType* declaredArray = static_cast<const ArrayType*>(declaredType);
            if (declaredArray->isDynamic() && stmt->initializer &&
                stmt->initializer->type == ASTNode::NodeType::ARRAY_LITERAL) {
                auto* arrayLit = static_cast<ArrayLiteralExpr*>(stmt->initializer.get());
                int inferredSize = static_cast<int>(arrayLit->elements.size());
                declaredType = typeSystem->getArrayType(
                    const_cast<Type*>(declaredArray->getElementType()), inferredSize);
            }
        }

        // CRITICAL: Update typeName to resolved type name for IR generation
        // For generic structs, this will be the mangled name (_Aria_M_Box_<hash>_int64)
        // For other types, this will be the canonical name (int64, string, etc.)
        stmt->typeName = declaredType->toString();

        // FUNCTION_TYPE: function pointer variable — IR generator handles all codegen.
        // Register as unknown (so call sites don't error) and skip all type checks.
        if (stmt->typeNode->type == ASTNode::NodeType::FUNCTION_TYPE) {
            symbolTable->defineSymbol(stmt->varName,
                                     SymbolKind::VARIABLE,
                                     typeSystem->getUnknownType(),
                                     stmt->line, stmt->column);
            return;
        }
    } else {
        // Legacy path: Get declared type from typeName string
        // Try struct first to avoid auto-creating primitive types
        declaredType = typeSystem->getStructType(stmt->typeName);
        
        if (!declaredType) {
            // Try primitive type
            declaredType = typeSystem->getPrimitiveType(stmt->typeName);
            
            if (!declaredType || declaredType->getKind() == TypeKind::ERROR) {
                std::string errorMsg = "Unknown type: '" + stmt->typeName + "'";
                std::string suggestion = findSimilarType(stmt->typeName);
                if (!suggestion.empty()) {
                    errorMsg += ". Did you mean '" + suggestion + "'?";
                    std::string lower = stmt->typeName;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    if (lower == suggestion || lower == "nil") {
                        errorMsg += " (Aria types are lowercase)";
                    }
                }
                addError(errorMsg, stmt);
                return;
            }
        }
    }
    
    // ARIA-024: int128/int256/int512 now use Limb-Based Integral Model (LBIM)
    // The semantic block for these types has been removed as they are now
    // represented as structs of i64 limbs, bypassing LLVM IPSCCP bugs.

    // Check const variables have initializers
    if (stmt->isConst && !stmt->initializer) {
        addError("const variable '" + stmt->varName + "' must have initializer", stmt);
        return;
    }
    
    // If initializer exists, check type compatibility
    if (stmt->initializer) {
        Type* initType = inferType(stmt->initializer.get());
        
        if (initType->getKind() == TypeKind::ERROR) {
            // Error already reported by inferType
            return;
        }
        
        // ========================================================================
        // NIL vs NULL Type Safety Enforcement
        // NIL can ONLY be used with optional types (T?)
        // NULL can ONLY be used with pointer types (T@)
        // ========================================================================
        if (stmt->initializer->type == ASTNode::NodeType::LITERAL) {
            LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
            
            // Check NIL literal - must be used with optional types only
            if (literal->explicit_type == "NIL") {
                if (declaredType->getKind() != TypeKind::OPTIONAL) {
                    addError("NIL can only be assigned to optional types (T?). " 
                            "Variable '" + stmt->varName + "' has type '" + 
                            declaredType->toString() + "'. Use NULL for pointers.", stmt);
                    return;
                }
            }
            
            // Check NULL literal - must be used with pointer types only
            if (literal->explicit_type == "NULL") {
                if (declaredType->getKind() != TypeKind::POINTER) {
                    addError("NULL can only be assigned to pointer types (T@). " 
                            "Variable '" + stmt->varName + "' has type '" + 
                            declaredType->toString() + "'. Use NIL for optionals.", stmt);
                    return;
                }
            }
        }
        
        // TBB Type Validation (Phase 3.2.4)
        // Special handling for integer values (literals or expressions) assigned to TBB types
        // Allows: tbb8:x = 100; and tbb8:x = 10 + 20;
        // This is safe because:
        //   1. int32 has smaller range than most TBB types (except tbb8)
        //   2. Range check will validate at compile time if possible
        //   3. Runtime check inserted if value not known at compile time
        bool tbbLiteralAssignment = false;
        if (isTBBType(declaredType)) {
            // Allow int32 → tbb coercion for literals and simple expressions
            // This enables natural syntax: tbb8:x = 42;
            if (initType->toString() == "int32" || initType->toString() == "int64") {
                // Check if it's a compile-time constant literal
                if (stmt->initializer->type == ASTNode::NodeType::LITERAL) {
                    LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
                    if (std::holds_alternative<int64_t>(literal->value)) {
                        int64_t value = std::get<int64_t>(literal->value);
                        checkTBBLiteralValue(value, declaredType, stmt);
                        tbbLiteralAssignment = true;
                        
                        // If no errors, allow the assignment (literal is in range)
                        if (hasErrors()) {
                            return;  // Validation failed
                        }
                    }
                } else {
                    // Expression result (e.g., 10 + 20) - allow with implicit coercion
                    // The actual range check will happen at runtime
                    // TODO: Add compile-time constant folding to catch more errors
                    tbbLiteralAssignment = true;
                }
            }
        }
        
        // ERR Literal Validation
        // Special handling for ERR literals assigned to TBB types
        bool errLiteralAssignment = false;
        if (isTBBType(declaredType) && stmt->initializer->type == ASTNode::NodeType::LITERAL) {
            LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
            if (std::holds_alternative<std::string>(literal->value) && 
                std::get<std::string>(literal->value) == "ERR") {
                // ERR is valid for any TBB type
                // The actual value (-128 for tbb8, -32768 for tbb16, etc.) will be 
                // determined in IR generation based on the target type
                errLiteralAssignment = true;
                // No validation needed - ERR is always valid for TBB types
            }
        }
        
        // Balanced Type Validation (Phase 3.2.5)
        // Special handling for integer literals assigned to balanced types
        bool balancedLiteralAssignment = false;
        if (isBalancedType(declaredType)) {
            // Check for direct literal
            if (stmt->initializer->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
                if (std::holds_alternative<int64_t>(literal->value)) {
                    int64_t value = std::get<int64_t>(literal->value);
                    checkBalancedLiteralValue(value, declaredType, stmt);
                    balancedLiteralAssignment = true;
                    
                    // If no errors, allow the assignment (literal is in range)
                    if (hasErrors()) {
                        return;  // Validation failed
                    }
                }
            }
            // Check for negative literal (unary minus applied to literal)
            else if (stmt->initializer->type == ASTNode::NodeType::UNARY_OP) {
                UnaryExpr* unary = static_cast<UnaryExpr*>(stmt->initializer.get());
                if (unary->op.type == frontend::TokenType::TOKEN_MINUS &&
                    unary->operand->type == ASTNode::NodeType::LITERAL) {
                    LiteralExpr* literal = static_cast<LiteralExpr*>(unary->operand.get());
                    if (std::holds_alternative<int64_t>(literal->value)) {
                        int64_t value = -std::get<int64_t>(literal->value);  // Negate
                        checkBalancedLiteralValue(value, declaredType, stmt);
                        balancedLiteralAssignment = true;
                        
                        // If no errors, allow the assignment (literal is in range)
                        if (hasErrors()) {
                            return;  // Validation failed
                        }
                    }
                }
            }
        }
        
        // Numeric Exotic Type Validation (Phase 3.3)  
        // Special handling for integer literals assigned to frac*, tfp*, vec9, etc.
        bool numericExoticLiteralAssignment = false;
        if (isNumericExoticType(declaredType)) {
            // Check for direct literal
            if (stmt->initializer->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
                if (std::holds_alternative<int64_t>(literal->value) || 
                    std::holds_alternative<double>(literal->value)) {
                    // Allow integer or float literals to initialize exotic numeric types
                    // The IR generator will create the appropriate struct representation
                    numericExoticLiteralAssignment = true;
                }
            }
            // Check for negative literal (unary minus applied to literal)
            else if (stmt->initializer->type == ASTNode::NodeType::UNARY_OP) {
                UnaryExpr* unary = static_cast<UnaryExpr*>(stmt->initializer.get());
                if (unary->op.type == frontend::TokenType::TOKEN_MINUS &&
                    unary->operand->type == ASTNode::NodeType::LITERAL) {
                    LiteralExpr* literal = static_cast<LiteralExpr*>(unary->operand.get());
                    if (std::holds_alternative<int64_t>(literal->value) ||
                        std::holds_alternative<double>(literal->value)) {
                        // Allow negative literals for exotic numeric types
                        numericExoticLiteralAssignment = true;
                    }
                }
            }
        }
        
        // High-Precision Literal Conversion (Phase 3.2.5)
        // Handle raw literal strings when target type is known
        bool highPrecisionLiteralAssignment = false;
        if (stmt->initializer->type == ASTNode::NodeType::LITERAL) {
            LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
            if (literal->hasRawValue()) {
                // We have a high-precision literal and know the target type
                // Validate the conversion is possible
                const std::string& raw = literal->getRawValue();
                std::string typeName = declaredType->toString();
                
                // Check if target type is a high-precision type
                if (typeName == "flt128" || typeName == "flt256" || typeName == "flt512" ||
                    typeName == "int128" || typeName == "int256" || typeName == "int512" ||
                    typeName == "uint128" || typeName == "uint256" || typeName == "uint512") {
                    
                    // Validate that the literal can be converted
                    // The actual conversion will happen in IR generation
                    bool is_float = (raw.find('.') != std::string::npos || 
                                    raw.find('e') != std::string::npos ||
                                    raw.find('E') != std::string::npos);
                    
                    bool type_is_float = (typeName.find("flt") == 0);
                    
                    if (is_float && !type_is_float) {
                        addError("Cannot assign float literal to integer type '" + typeName + "'", stmt);
                        return;
                    }
                    
                    if (!is_float && type_is_float) {
                        // Integer to float is okay (will be converted)
                    }
                    
                    highPrecisionLiteralAssignment = true;
                }
            }
        }
        
        // Standard Integer Type Validation
        // Special handling for integer literals assigned to smaller integer types
        // This allows safe narrowing conversions when the literal value fits at compile time
        bool standardIntLiteralAssignment = false;
        if (isStandardIntType(declaredType)) {
            // Check if initializer is a direct literal
            if (stmt->initializer->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
                if (std::holds_alternative<int64_t>(literal->value)) {
                    int64_t value = std::get<int64_t>(literal->value);
                    if (canLiteralFitInIntType(value, declaredType, stmt)) {
                        standardIntLiteralAssignment = true;
                        // Literal fits in target type, allow the assignment
                    } else {
                        return;  // Validation failed, error already reported
                    }
                }
            }
            // Check if initializer is a unary minus on a literal (negative number)
            else if (stmt->initializer->type == ASTNode::NodeType::UNARY_OP) {
                UnaryExpr* unaryExpr = static_cast<UnaryExpr*>(stmt->initializer.get());
                if (unaryExpr->op.type == TokenType::TOKEN_MINUS && 
                    unaryExpr->operand->type == ASTNode::NodeType::LITERAL) {
                    LiteralExpr* literal = static_cast<LiteralExpr*>(unaryExpr->operand.get());
                    if (std::holds_alternative<int64_t>(literal->value)) {
                        int64_t value = -std::get<int64_t>(literal->value);  // Apply negation
                        if (canLiteralFitInIntType(value, declaredType, stmt)) {
                            standardIntLiteralAssignment = true;
                            // Literal fits in target type, allow the assignment
                        } else {
                            return;  // Validation failed, error already reported
                        }
                    }
                }
            }
        }
        
        // Array Literal Integer Coercion
        // Special handling for array literals with integer elements assigned to different integer array types
        // Example: int64[]:nums = [10, 20, 30];  // Literals are int32, but should fit in int64
        bool arrayLiteralIntCoercion = false;
        if (declaredType->getKind() == TypeKind::POINTER && 
            initType->getKind() == TypeKind::POINTER &&
            stmt->initializer->type == ASTNode::NodeType::ARRAY_LITERAL) {
            
            const PointerType* declaredPtr = static_cast<const PointerType*>(declaredType);
            const PointerType* initPtr = static_cast<const PointerType*>(initType);
            
            Type* declaredElem = declaredPtr->getPointeeType();
            Type* initElem = initPtr->getPointeeType();
            
            // Check if both element types are standard integers
            if (isStandardIntType(declaredElem) && isStandardIntType(initElem)) {
                ArrayLiteralExpr* arrayLit = static_cast<ArrayLiteralExpr*>(stmt->initializer.get());
                
                // Check if all elements are integer literals that fit in the declared element type
                bool allFit = true;
                for (const auto& elem : arrayLit->elements) {
                    if (elem->type != ASTNode::NodeType::LITERAL) {
                        allFit = false;
                        break;
                    }
                    
                    LiteralExpr* literal = static_cast<LiteralExpr*>(elem.get());
                    if (!std::holds_alternative<int64_t>(literal->value)) {
                        allFit = false;
                        break;
                    }
                    
                    int64_t value = std::get<int64_t>(literal->value);
                    if (!integerFitsInType(value, declaredElem->toString())) {
                        allFit = false;
                        break;
                    }
                }
                
                if (allFit) {
                    arrayLiteralIntCoercion = true;
                    // All literals fit, allow the assignment
                }
            }
        }
        
        // P2.7: Array Literal Object Literal Coercion
        // Special handling for array literals with object literal elements assigned to struct array types
        // Example: Point[2]:points = [{x: 1, y: 2}, {x: 3, y: 4}];
        bool arrayLiteralObjectCoercion = false;
        if (declaredType->getKind() == TypeKind::ARRAY &&
            stmt->initializer->type == ASTNode::NodeType::ARRAY_LITERAL) {
            
            const ArrayType* declaredArray = static_cast<const ArrayType*>(declaredType);
            Type* declaredElem = declaredArray->getElementType();
            
            // Check if element type is a struct
            if (declaredElem->getKind() == TypeKind::STRUCT) {
                ArrayLiteralExpr* arrayLit = static_cast<ArrayLiteralExpr*>(stmt->initializer.get());
                const StructType* structType = static_cast<const StructType*>(declaredElem);
                
                // Check if all array elements are object literals
                bool allObjectLiterals = true;
                for (const auto& elem : arrayLit->elements) {
                    if (elem->type != ASTNode::NodeType::OBJECT_LITERAL) {
                        allObjectLiterals = false;
                        break;
                    }
                }
                
                if (allObjectLiterals) {
                    // Mark each object literal with the struct type name
                    const auto& structFields = structType->getFields();
                    
                    for (const auto& elem : arrayLit->elements) {
                        ObjectLiteralExpr* objLit = static_cast<ObjectLiteralExpr*>(elem.get());
                        
                        // Validate field count
                        if (objLit->fields.size() != structFields.size()) {
                            addError("Object literal in array has " + std::to_string(objLit->fields.size()) + 
                                    " fields, but struct '" + structType->getName() + 
                                    "' requires " + std::to_string(structFields.size()) + " fields", elem.get());
                            return;
                        }
                        
                        // Validate each field's type
                        for (size_t i = 0; i < objLit->fields.size(); ++i) {
                            const auto& litField = objLit->fields[i];
                            const auto& structField = structFields[i];
                            
                            // Infer the field value's type
                            Type* fieldValueType = inferType(litField.value.get());
                            if (fieldValueType->getKind() == TypeKind::ERROR) {
                                return;  // Error already reported
                            }
                            
                            // Check type compatibility
                            if (!fieldValueType->isAssignableTo(structField.type) && 
                                !canCoerce(fieldValueType, structField.type)) {
                                addError("Field '" + structField.name + "' of struct '" + 
                                        structType->getName() + "' in array element expects type '" + 
                                        structField.type->toString() + "', but got '" + 
                                        fieldValueType->toString() + "'", litField.value.get());
                                return;
                            }
                        }
                        
                        // Mark the object literal with the struct type name for IR generation
                        objLit->type_name = structType->getName();
                    }
                    
                    arrayLiteralObjectCoercion = true;
                }
            }
        }
        
        // Optional Type Integer Literal Coercion
        // Special handling for integer literals assigned to optional integer types
        // Example: int64?:opt = 42;  // 42 is int32, but should fit in int64
        bool optionalIntLiteralCoercion = false;
        if (declaredType->getKind() == TypeKind::OPTIONAL && 
            stmt->initializer->type == ASTNode::NodeType::LITERAL) {
            
            const OptionalType* declaredOptional = static_cast<const OptionalType*>(declaredType);
            Type* wrappedType = declaredOptional->getWrappedType();
            
            if (isStandardIntType(wrappedType)) {
                LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
                if (std::holds_alternative<int64_t>(literal->value)) {
                    int64_t value = std::get<int64_t>(literal->value);
                    if (integerFitsInType(value, wrappedType->toString())) {
                        optionalIntLiteralCoercion = true;
                        // Literal fits in wrapped type, allow the assignment
                    }
                }
            }
        }
        
        // Object Literal Struct Initialization (Phase 3.4)
        // Special handling for positional object literals assigned to struct-like types
        // This includes: frac8/16/32/64 (3 fields), tfp32/64 (2 fields), vec9 (9 fields), etc.
        // Example: frac8:half = {0, 1, 2};  // {whole, num, denom}
        bool objectLiteralStructAssignment = false;
        if (stmt->initializer->type == ASTNode::NodeType::OBJECT_LITERAL) {
            ObjectLiteralExpr* objLit = static_cast<ObjectLiteralExpr*>(stmt->initializer.get());
            
            // Check if declared type is a struct type
            if (declaredType->getKind() == TypeKind::STRUCT) {
                const StructType* structType = static_cast<const StructType*>(declaredType);
                
                // Get struct fields
                const std::vector<StructType::Field>& structFields = structType->getFields();
                
                // Check field count matches
                if (objLit->fields.size() != structFields.size()) {
                    addError("Object literal has " + std::to_string(objLit->fields.size()) + 
                            " fields, but struct '" + structType->getName() + 
                            "' requires " + std::to_string(structFields.size()) + " fields", stmt);
                    return;
                }
                
                // Validate each field's type
                for (size_t i = 0; i < objLit->fields.size(); ++i) {
                    const auto& litField = objLit->fields[i];
                    const auto& structField = structFields[i];
                    
                    // P2.7: Special handling for array literal fields with object literal elements
                    // If field expects array of structs, mark object literals with struct type name
                    if (litField.value->type == ASTNode::NodeType::ARRAY_LITERAL &&
                        structField.type->getKind() == TypeKind::ARRAY) {
                        
                        const ArrayType* expectedArrayType = static_cast<const ArrayType*>(structField.type);
                        Type* expectedElemType = expectedArrayType->getElementType();
                        
                        if (expectedElemType->getKind() == TypeKind::STRUCT) {
                            const StructType* expectedStructType = static_cast<const StructType*>(expectedElemType);
                            ArrayLiteralExpr* arrayLit = static_cast<ArrayLiteralExpr*>(litField.value.get());
                            
                            // Mark each object literal element with the struct type name
                            for (const auto& elem : arrayLit->elements) {
                                if (elem->type == ASTNode::NodeType::OBJECT_LITERAL) {
                                    ObjectLiteralExpr* elemObjLit = static_cast<ObjectLiteralExpr*>(elem.get());
                                    elemObjLit->type_name = expectedStructType->getName();
                                }
                            }
                        }
                    }
                    
                    // Infer the field value's type
                    Type* fieldValueType = inferType(litField.value.get());
                    if (fieldValueType->getKind() == TypeKind::ERROR) {
                        return;  // Error already reported
                    }
                    
                    // Check type compatibility (allow int32/int64 literals)
                    bool compatible = false;
                    if (fieldValueType->isAssignableTo(structField.type)) {
                        compatible = true;
                    } else if (canCoerce(fieldValueType, structField.type)) {
                        compatible = true;
                    } else if ((fieldValueType->toString() == "int32" || fieldValueType->toString() == "int64") &&
                               isStandardIntType(structField.type)) {
                        // Allow integer literal coercion for standard int struct fields
                        if (litField.value->type == ASTNode::NodeType::LITERAL) {
                            LiteralExpr* lit = static_cast<LiteralExpr*>(litField.value.get());
                            if (std::holds_alternative<int64_t>(lit->value)) {
                                int64_t value = std::get<int64_t>(lit->value);
                                compatible = integerFitsInType(value, structField.type->toString());
                            }
                        }
                    } else if ((fieldValueType->toString() == "int32" || fieldValueType->toString() == "int64") &&
                               isBalancedType(structField.type)) {
                        // Allow integer literal coercion for balanced type struct fields (trit, nit, tryte, nyte)
                        // Only when the value is unambiguous (fits in the target type's range)
                        
                        // Handle both direct literals and unary negation of literals
                        int64_t value = 0;
                        bool hasValue = false;
                        
                        if (litField.value->type == ASTNode::NodeType::LITERAL) {
                            LiteralExpr* lit = static_cast<LiteralExpr*>(litField.value.get());
                            if (std::holds_alternative<int64_t>(lit->value)) {
                                value = std::get<int64_t>(lit->value);
                                hasValue = true;
                            }
                        } else if (litField.value->type == ASTNode::NodeType::UNARY_OP) {
                            // Handle unary negation: -2, -1, etc.
                            UnaryExpr* unary = static_cast<UnaryExpr*>(litField.value.get());
                            if (unary->op.type == TokenType::TOKEN_MINUS && unary->operand->type == ASTNode::NodeType::LITERAL) {
                                LiteralExpr* lit = static_cast<LiteralExpr*>(unary->operand.get());
                                if (std::holds_alternative<int64_t>(lit->value)) {
                                    value = -std::get<int64_t>(lit->value);  // Negate the literal
                                    hasValue = true;
                                }
                            }
                        }
                        
                        if (hasValue) {
                            // Check if value fits in the target balanced type
                            if (isNitType(structField.type)) {
                                // nit: [-4, 4] or -128 (ERR sentinel)
                                compatible = ((value >= -4 && value <= 4) || value == -128);
                            } else if (isTritType(structField.type)) {
                                // trit: {-1, 0, 1} or -128 (ERR sentinel)
                                compatible = (value == -1 || value == 0 || value == 1 || value == -128);
                            } else if (isNyteType(structField.type) || isTryteType(structField.type)) {
                                // tryte/nyte: [-29524, 29524]
                                compatible = (value >= -29524 && value <= 29524);
                            }
                        }
                    }
                    
                    if (!compatible) {
                        addError("Field '" + structField.name + "' of struct '" + 
                                structType->getName() + "' expects type '" + 
                                structField.type->toString() + "', but got '" + 
                                fieldValueType->toString() + "'", litField.value.get());
                        return;
                    }
                }
                
                // Mark the object literal with the struct type name for IR generation
                objLit->type_name = structType->getName();
                
                // Validation passed
                objectLiteralStructAssignment = true;
            }
            // Check if declared type is an exotic numeric type with struct-like initialization
            else if (isNumericExoticType(declaredType) || isBalancedType(declaredType) || isTBBType(declaredType)) {
                PrimitiveType* primType = dynamic_cast<PrimitiveType*>(declaredType);
                if (primType) {
                    const std::string& typeName = primType->getName();
                    
                    // Determine expected field count based on type
                    size_t expectedFieldCount = 0;
                    if (typeName.find("frac") == 0) {
                        expectedFieldCount = 3;  // {whole, num, denom}
                    } else if (typeName.find("tfp") == 0) {
                        expectedFieldCount = 2;  // {exponent, mantissa}
                    } else if (typeName == "vec9" || typeName == "dvec9") {
                        expectedFieldCount = 9;  // 9 components
                    } else if (typeName == "tmatrix") {
                        // tmatrix can have variable size - just accept any field count for now
                        expectedFieldCount = objLit->fields.size();
                    } else if (typeName == "ttensor") {
                        // ttensor can have variable size - just accept any field count for now
                        expectedFieldCount = objLit->fields.size();
                    }
                    
                    // Check field count
                    if (expectedFieldCount > 0 && objLit->fields.size() != expectedFieldCount) {
                        addError("Object literal has " + std::to_string(objLit->fields.size()) + 
                                " fields, but type '" + typeName + 
                                "' requires " + std::to_string(expectedFieldCount) + " fields", stmt);
                        return;
                    }
                    
                    // For now, just accept int32/int64 literals for all fields
                    // More detailed type checking can be added later
                    for (const auto& litField : objLit->fields) {
                        Type* fieldValueType = inferType(litField.value.get());
                        if (fieldValueType->getKind() == TypeKind::ERROR) {
                            return;  // Error already reported
                        }
                        
                        // Allow integer literals
                        std::string fieldTypeStr = fieldValueType->toString();
                        if (fieldTypeStr != "int32" && fieldTypeStr != "int64" && 
                            fieldTypeStr != "tbb8" && fieldTypeStr != "tbb16" && 
                            fieldTypeStr != "tbb32" && fieldTypeStr != "tbb64") {
                            addError("Field value in '" + typeName + "' object literal must be an integer, got '" + 
                                    fieldTypeStr + "'", litField.value.get());
                            return;
                        }
                    }
                    
                    // Mark the object literal with the type name for IR generation
                    objLit->type_name = typeName;
                    
                    // Validation passed
                    objectLiteralStructAssignment = true;
                }
            }
        }
        
        // Check if initializer type is assignable to declared type
        // Skip this check if we handled TBB, ERR, balanced, numeric exotic, standard int, high-precision literal, array literal int/object, optional int literal, or object literal struct coercion above
        if (!tbbLiteralAssignment && !errLiteralAssignment && !balancedLiteralAssignment && !numericExoticLiteralAssignment && !standardIntLiteralAssignment && !highPrecisionLiteralAssignment && !arrayLiteralIntCoercion && !arrayLiteralObjectCoercion && !optionalIntLiteralCoercion && !objectLiteralStructAssignment) {
            // Special case: Allow T to be initialized to T? (wrapping)
            bool optionalWrapping = false;
            if (declaredType->getKind() == TypeKind::OPTIONAL) {
                const OptionalType* declaredOptional = static_cast<const OptionalType*>(declaredType);
                if (initType->isAssignableTo(declaredOptional->getWrappedType())) {
                    optionalWrapping = true;
                }
            }
            
            // BUG-003 FIX: Implicit Result<T> → T unwrapping is FORBIDDEN.
            // Every function with a body returns Result<T>. The caller must
            // explicitly handle the result — either by:
            //   1. Declaring the variable as Result<T> and checking .is_error
            //   2. Using raw(expr) to explicitly assert "I know this won't fail"
            // Silent unwrapping violates Aria's safety hierarchy and hides
            // error paths from auditors, lawyers, and the type system.
            if (initType->getKind() == TypeKind::RESULT) {
                const ResultType* resultType = static_cast<const ResultType*>(initType);
                Type* innerType = resultType->getValueType();
                if (innerType->isAssignableTo(declaredType)) {
                    addError("Cannot silently unwrap Result<" + declaredType->toString() + "> "
                            "into '" + stmt->varName + "' of type '" + declaredType->toString() + "'. "
                            "Declare as Result<" + declaredType->toString() + "> and check .is_error, "
                            "or use raw(expr) to explicitly assert the call cannot fail.", stmt);
                    return;
                }
            }
            bool resultUnwrapping = false;  // never set; kept to avoid restructuring the condition below
            
            // P1.5: Allow automatic void* ↔ wild T-> conversions at FFI boundaries
            bool ffiPointerConversion = canConvertFFIPointer(initType, declaredType);
            
            if (!initType->isAssignableTo(declaredType) && !canCoerce(initType, declaredType) && 
                !optionalWrapping && !resultUnwrapping && !ffiPointerConversion) {
                addError("Cannot initialize variable '" + stmt->varName + 
                        "' of type '" + declaredType->toString() + 
                        "' with value of type '" + initType->toString() + "'", stmt);
                return;
            }
        }
    }
    
    // Phase 2.2: Evaluate const expressions at compile time
    ComptimeValue* evaluatedConstValue = nullptr;
    if (stmt->isConst && stmt->initializer) {
        ComptimeValue constValue = constEvaluator->evaluate(stmt->initializer.get());
        // Store evaluated value on heap so it persists
        evaluatedConstValue = new ComptimeValue(constValue);
    }
    
    // Define symbol in symbol table
    symbolTable->defineSymbol(stmt->varName, 
                             stmt->isConst ? SymbolKind::CONSTANT : SymbolKind::VARIABLE,
                             declaredType, 
                             stmt->line, 
                             stmt->column);
    
    // Set fixed flag if this is a fixed variable
    if (stmt->isFixed) {
        Symbol* sym = symbolTable->resolveSymbol(stmt->varName);
        if (sym) {
            sym->isFixed = true;
        }
    }
    
    // Set borrow flags if this is a borrow variable ($$i/$$m)
    if (stmt->isBorrowImm || stmt->isBorrowMut) {
        Symbol* sym = symbolTable->resolveSymbol(stmt->varName);
        if (sym) {
            sym->isBorrowImm = stmt->isBorrowImm;
            sym->isBorrowMut = stmt->isBorrowMut;
        }
    }
    
    // If we evaluated a const value, store it in the symbol
    if (evaluatedConstValue) {
        Symbol* sym = symbolTable->resolveSymbol(stmt->varName);
        if (sym) {
            sym->setComptimeValue(evaluatedConstValue);
        }
    }
    
    // Phase 1.5: Track wild Result variables
    // Wild Results can modify .is_error field (allows error state corruption)
    // Non-wild Results have immutable .is_error (errors must propagate)
    if (declaredType->getKind() == TypeKind::RESULT && stmt->isWild) {
        wildResults.insert(stmt->varName);
    }
    
    // v0.2.41: Validate limit<> constraints
    if (!stmt->limitRulesName.empty()) {
        auto it = rulesTable.find(stmt->limitRulesName);
        if (it == rulesTable.end()) {
            addError("Unknown rules '" + stmt->limitRulesName + "' in limit<>. "
                     "Declare Rules:" + stmt->limitRulesName + " before using it.", stmt);
            return;
        }
        
        RulesDeclStmt* rulesDecl = it->second;
        
        // v0.2.42: Type parameter enforcement
        // v0.2.44: Also matches array types (T[] matches T[N]) and pointer types (T@ matches T@)
        if (!rulesDecl->typeParams.empty()) {
            // Rules has type params — variable type must match one of them
            std::string varTypeName = declaredType->toString();
            bool typeMatch = false;
            for (const auto& tp : rulesDecl->typeParams) {
                if (tp == varTypeName) {
                    typeMatch = true;
                    break;
                }
                // v0.2.44: Array type matching — "int8[]" matches "int8[N]" for any N
                if (tp.length() > 2 && tp.substr(tp.length()-2) == "[]") {
                    std::string baseType = tp.substr(0, tp.length()-2);
                    // varTypeName should be "baseType[N]" for some N
                    if (varTypeName.length() > baseType.length() + 2 &&
                        varTypeName.substr(0, baseType.length()) == baseType &&
                        varTypeName[baseType.length()] == '[' &&
                        varTypeName.back() == ']') {
                        typeMatch = true;
                        break;
                    }
                }
            }
            if (!typeMatch) {
                std::string allowed;
                for (size_t i = 0; i < rulesDecl->typeParams.size(); ++i) {
                    if (i > 0) allowed += ", ";
                    allowed += rulesDecl->typeParams[i];
                }
                addError("limit<" + stmt->limitRulesName + "> can only be applied to " + 
                         allowed + ", got '" + varTypeName + "'", stmt);
                return;
            }
        } else {
            // No type params (backward compat) — only numeric types allowed
            if (!isNumericType(declaredType)) {
                addError("limit<> constraints can only be applied to numeric types, not '" + 
                         declaredType->toString() + "'", stmt);
                return;
            }
        }
        
        // Track this variable as limited for assignment checking
        limitedVariables[stmt->varName] = stmt->limitRulesName;
        
        // Compile-time check: if initializer is a literal, evaluate rules now
        if (stmt->initializer && stmt->initializer->type == ASTNode::NodeType::LITERAL) {
            LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->initializer.get());
            if (std::holds_alternative<int64_t>(literal->value)) {
                int64_t value = std::get<int64_t>(literal->value);
                validateLimitRules(stmt->limitRulesName, value, stmt);
            } else if (std::holds_alternative<double>(literal->value)) {
                double value = std::get<double>(literal->value);
                validateLimitRulesFloat(stmt->limitRulesName, value, stmt);
            }
        }
        // Compile-time check: negative literal (unary minus on literal)
        else if (stmt->initializer && stmt->initializer->type == ASTNode::NodeType::UNARY_OP) {
            UnaryExpr* unary = static_cast<UnaryExpr*>(stmt->initializer.get());
            if (unary->op.type == frontend::TokenType::TOKEN_MINUS &&
                unary->operand->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* literal = static_cast<LiteralExpr*>(unary->operand.get());
                if (std::holds_alternative<int64_t>(literal->value)) {
                    int64_t value = -std::get<int64_t>(literal->value);
                    validateLimitRules(stmt->limitRulesName, value, stmt);
                } else if (std::holds_alternative<double>(literal->value)) {
                    double value = -std::get<double>(literal->value);
                    validateLimitRulesFloat(stmt->limitRulesName, value, stmt);
                }
            }
        }
        // Dynamic value: runtime checks will be inserted by IR generator
    }
}

// ============================================================================
// P1-4: Design by Contract - Contract Validation
// ============================================================================

void TypeChecker::validateContracts(const std::vector<ASTNodePtr>& contracts,
                                   bool isPostcondition,
                                   Type* valueType,
                                   ASTNode* stmt) {
    // If no contracts, nothing to validate
    if (contracts.empty()) {
        return;
    }
    
    // For postconditions, temporarily define 'result' symbol
    // This allows postconditions to reference the return value
    Symbol* resultSymbol = nullptr;
    if (isPostcondition && valueType) {
        // Check if 'result' is already defined (would be a parameter name conflict)
        Symbol* existing = symbolTable->lookupSymbol("result");
        if (existing) {
            addError("Cannot use 'result' as parameter name - reserved for postconditions", stmt);
        } else {
            resultSymbol = symbolTable->defineSymbol("result", SymbolKind::VARIABLE,
                                                    valueType, stmt->line, stmt->column);
        }
    }
    
    // Validate each contract clause
    for (size_t i = 0; i < contracts.size(); ++i) {
        ASTNodePtr contract = contracts[i];
        if (!contract) {
            addError("Contract clause is null", stmt);
            continue;
        }
        
        // Infer the type of the contract expression
        Type* contractType = inferType(contract.get());
        
        if (!contractType) {
            std::string clauseType = isPostcondition ? "ensures" : "requires";
            addError("Failed to infer type for " + clauseType + " clause #" + 
                    std::to_string(i + 1), stmt);
            continue;
        }
        
        // Contract expressions must evaluate to boolean
        // bool is a primitive type with name "bool"
        bool isBool = false;
        if (contractType->getKind() == TypeKind::PRIMITIVE) {
            PrimitiveType* primType = static_cast<PrimitiveType*>(contractType);
            isBool = (primType->getName() == "bool");
        }
        
        if (!isBool) {
            std::string clauseType = isPostcondition ? "ensures" : "requires";
            addError("Contract " + clauseType + " clause #" + std::to_string(i + 1) + 
                    " must be boolean expression, got " + contractType->toString(), stmt);
        }
        
        // TODO Phase 2.5: Check for side effects (no function calls with side effects)
        // For now, just validate types
    }
    
    // Note: resultSymbol will be automatically cleaned up when function scope exits
    // No need to manually remove it
}

// ============================================================================
// Function Declaration Type Checking
// ============================================================================

void TypeChecker::checkFuncDecl(FuncDeclStmt* stmt) {
    // Skip detailed type checking for generic functions
    // Their bodies will be checked during monomorphization when types are concrete
    if (!stmt->genericParams.empty()) {
        // Register the generic function in the symbol table so it can be found during calls
        // Create a placeholder function type (will be specialized during monomorphization)
        std::vector<Type*> placeholderParams;
        for (const auto& param : stmt->parameters) {
            placeholderParams.push_back(typeSystem->getUnknownType());
        }
        Type* placeholderReturn = typeSystem->getUnknownType();
        Type* funcType = new FunctionType(placeholderParams, placeholderReturn, stmt->isAsync, false);
        
        Symbol* funcSymbol = symbolTable->defineSymbol(stmt->funcName, SymbolKind::FUNCTION,
                                                        funcType, stmt->line, stmt->column);
        if (funcSymbol) {
            funcSymbol->setFuncDecl(stmt);  // Store AST for generic resolution
        }
        
        // Don't check the body - it will be checked during monomorphization
        return;
    }
    
    // Register comptime functions with the ConstEvaluator for CTFE
    if (stmt->isComptime) {
        constEvaluator->registerFunction(stmt->funcName, stmt);
    }
    
    // Resolve return type from the type node
    Type* valueType = resolveTypeNode(stmt->returnType.get());
    
    if (!valueType) {
        if (stmt->returnType) {
            addError("Unknown return type: '" + stmt->returnType->toString() + "'", stmt);
        } else {
            addError("Missing return type", stmt);
        }
        return;
    }

    // Validate main function signature
    // main must return int32 to be compatible with C runtime expectations
    if (stmt->funcName == "main") {
        std::string returnTypeName = valueType->toString();
        if (returnTypeName != "int32") {
            addError("Function 'main' must return 'int32', but returns '" +
                     returnTypeName + "'", stmt);
            // Continue checking to report other errors, but main is invalid
        }

        // main should take no parameters (for now - could support int argc, char** argv later)
        if (!stmt->parameters.empty()) {
            addError("Function 'main' should take no parameters", stmt);
        }
    }

    // Validate failsafe function signature
    // failsafe must return int32 and take exactly one tbb32 parameter
    if (stmt->funcName == "failsafe") {
        std::string returnTypeName = valueType->toString();
        if (returnTypeName != "int32") {
            addError("Function 'failsafe' must return 'int32', but returns '" +
                     returnTypeName + "'. Use exit() to terminate, not pass().", stmt);
        }

        if (stmt->parameters.size() != 1) {
            addError("Function 'failsafe' must take exactly one parameter: tbb32:err_code", stmt);
        } else {
            // Check parameter type is tbb32
            auto& param = stmt->parameters[0];
            if (param->type == ASTNode::NodeType::PARAMETER) {
                ParameterNode* pn = static_cast<ParameterNode*>(param.get());
                if (pn->typeNode) {
                    std::string paramTypeName = pn->typeNode->toString();
                    if (paramTypeName != "tbb32") {
                        addError("Function 'failsafe' parameter must be tbb32, got '" +
                                 paramTypeName + "'. tbb32 allows ERR signal for unknown errors.", stmt);
                    }
                }
            }
        }
    }

    // ARIA-026 FIX: FFI Safety - Ban string return types from extern functions
    if (!stmt->body && valueType->toString() == "str") {  // No body = extern function
        addError("FFI Safety Violation: Cannot return 'str' from extern function '" + 
                 stmt->funcName + "'. Aria strings are fat pointers {data, len} incompatible " +
                 "with C char*. Use 'wild int8*' for C strings and convert with string_from_cstr().", 
                 stmt);
    }

    // Return type handling:
    // - Regular Aria functions return Result<T> for error handling
    // - Extern functions (no body) return raw T for C ABI compatibility
    
    // DESIGN ENFORCEMENT: void type only allowed in extern blocks
    // Aria uses NIL for no-value returns to force unfamiliarity and prevent C assumptions
    Type* voidType = typeSystem->getPrimitiveType("void");
    if (!stmt->body) {
        // Extern function: void is allowed (FFI boundary)
    } else if (valueType->equals(voidType)) {
        // Non-extern function: void is forbidden, use NIL instead
        addError("void return type only allowed in extern blocks. Use NIL for no-value returns in Aria code.", stmt);
        return;
    }
    
    Type* actualReturnType;
    if (!stmt->body) {
        // Extern function: return raw type (no Result wrapping)
        actualReturnType = valueType;
    } else {
        // Regular function: return Result<T>
        actualReturnType = new ResultType(valueType);
    }

    // Set current function return types for pass/fail/return statement checking
    Type* previousReturnType = currentFunctionReturnType;
    Type* previousValueType = currentFunctionValueType;
    std::string previousFunctionName = currentFunctionName;
    currentFunctionReturnType = actualReturnType;
    currentFunctionValueType = valueType;
    currentFunctionName = stmt->funcName;
    
    // PRE-REGISTER FUNCTION SYMBOL FOR SELF-RECURSION SUPPORT
    // Build a forward-reference symbol BEFORE entering the function scope so the
    // function name is visible inside the body.  This allows recursive calls like
    //   func:fib = int32(int32:n) { ... pass(fib(n-1i32)); ... }
    // to resolve correctly during type-checking.
    {
        std::vector<Type*> earlyParamTypes;
        for (const auto& param : stmt->parameters) {
            if (param->type == ASTNode::NodeType::PARAMETER) {
                ParameterNode* paramNode = static_cast<ParameterNode*>(param.get());
                Type* pt = resolveTypeNode(paramNode->typeNode.get());
                earlyParamTypes.push_back((pt && pt->getKind() != TypeKind::ERROR)
                    ? pt : typeSystem->getPrimitiveType("void"));
            }
        }
        Type* earlyFuncType = new FunctionType(earlyParamTypes, actualReturnType,
                                               stmt->isAsync, false);
        Symbol* earlySym = symbolTable->defineSymbol(stmt->funcName, SymbolKind::FUNCTION,
                                                      earlyFuncType,
                                                      stmt->line, stmt->column);
        if (earlySym) {
            earlySym->setFuncDecl(stmt);
        }
    }

    // CRITICAL FIX: Enter function scope BEFORE defining parameters
    // This prevents parameter name collisions between different functions
    // Without this, all parameters are registered in global scope and collide!
    symbolTable->enterScope(ScopeKind::FUNCTION, stmt->funcName);
    
    // Check parameters
    for (const auto& param : stmt->parameters) {
        if (param->type == ASTNode::NodeType::PARAMETER) {
            ParameterNode* paramNode = static_cast<ParameterNode*>(param.get());
            
            // Resolve parameter type from the type node
            Type* paramType = resolveTypeNode(paramNode->typeNode.get());
            
            if (!paramType) {
                if (paramNode->typeNode) {
                    addError("Unknown parameter type: '" + paramNode->typeNode->toString() + "'", param.get());
                } else {
                    addError("Missing parameter type", param.get());
                }
                continue;
            }
            
            // ARIA-026 FIX: FFI Safety - Ban strings in extern functions
            // String type is a fat pointer {data, len} struct which is incompatible with C ABI char*.
            // Passing string to extern causes parameter shift: C sees 'data' as arg1, 'len' as arg2.
            // This corrupts actuator force limits and sensor readings in robotic control.
            if (!stmt->body && paramType->toString() == "str") {  // No body = extern function
                addError("FFI Safety Violation: Cannot pass 'str' type to extern function '" + 
                         stmt->funcName + "'. Aria strings are fat pointers {data, len} incompatible " +
                         "with C char*. Use 'wild int8*' for C strings and convert with string_to_cstr().", 
                         param.get());
            }
            
            // Define parameter in symbol table (will be visible in function body)
            // FUNCTION_TYPE params (function pointer args) use UNKNOWN so call sites don't error.
            if (paramNode->typeNode &&
                paramNode->typeNode->type == ASTNode::NodeType::FUNCTION_TYPE) {
                symbolTable->defineSymbol(paramNode->paramName, SymbolKind::VARIABLE,
                                         typeSystem->getUnknownType(), param->line, param->column);
                continue;
            }
            symbolTable->defineSymbol(paramNode->paramName, SymbolKind::VARIABLE,
                                     paramType, param->line, param->column);
        }
    }
    
    // P1-4: Validate contract clauses (Design by Contract)
    // Check preconditions (requires) - can reference parameters only
    validateContracts(stmt->preconditions, false, valueType, stmt);
    
    // Check postconditions (ensures) - can reference parameters and 'result'
    validateContracts(stmt->postconditions, true, valueType, stmt);
    
    // Check function body (this is where generic calls get analyzed!)
    if (stmt->body) {
        checkStatement(stmt->body.get());
    }
    
    // Exit function scope AFTER body is checked
    symbolTable->exitScope();
    
    // Restore previous function return types
    currentFunctionReturnType = previousReturnType;
    currentFunctionValueType = previousValueType;
    currentFunctionName = previousFunctionName;
    
    // Note: The function symbol was pre-registered before the scope entry (above)
    // to support self-recursion.  No further defineSymbol() call needed here.
}

// ============================================================================
// Comptime Expression and Block Type Checking
// ============================================================================

Type* TypeChecker::inferComptimeExpr(ComptimeExpr* expr) {
    if (!expr || !expr->expr) {
        addError("Invalid comptime expression", expr);
        return typeSystem->getErrorType();
    }
    
    // First, type-check the inner expression normally
    Type* innerType = inferType(expr->expr.get());
    if (!innerType || innerType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // Evaluate the expression at compile time
    ComptimeValue result = constEvaluator->evaluate(expr->expr.get());
    
    // Check for evaluation errors
    if (constEvaluator->hasErrors()) {
        for (const auto& err : constEvaluator->getErrors()) {
            addError("comptime evaluation failed: " + err, expr);
        }
        constEvaluator->clearErrors();
        return typeSystem->getErrorType();
    }
    
    // Store the result on the AST node for the IR generator
    expr->evaluated = true;
    
    // Map the ComptimeValue kind back to a Type* and store result
    switch (result.getKind()) {
        case ComptimeValue::Kind::INTEGER:
        case ComptimeValue::Kind::UNSIGNED:
        case ComptimeValue::Kind::TBB: {
            expr->intResult = result.getInt();
            const std::string& typeName = result.getTypeName();
            expr->resultTypeName = typeName.empty() ? "int64" : typeName;
            Type* t = typeSystem->getPrimitiveType(expr->resultTypeName);
            return t ? t : typeSystem->getPrimitiveType("int64");
        }
        case ComptimeValue::Kind::FLOAT: {
            expr->floatResult = result.getFloat();
            const std::string& typeName = result.getTypeName();
            expr->resultTypeName = typeName.empty() ? "flt64" : typeName;
            Type* t = typeSystem->getPrimitiveType(expr->resultTypeName);
            return t ? t : typeSystem->getPrimitiveType("flt64");
        }
        case ComptimeValue::Kind::BOOL:
            expr->boolResult = result.getBool();
            expr->resultTypeName = "bool";
            return typeSystem->getPrimitiveType("bool");
        case ComptimeValue::Kind::STRING:
            expr->stringResult = result.getString();
            expr->resultTypeName = "str";
            return typeSystem->getPrimitiveType("str");
        default:
            // For complex types (arrays, structs, etc.), use the inner expression's type
            return innerType;
    }
}

void TypeChecker::checkComptimeBlock(ComptimeBlockStmt* stmt) {
    if (!stmt || !stmt->body) {
        addError("Invalid comptime block", stmt);
        return;
    }
    
    // Evaluate the block at compile time
    constEvaluator->evaluate(stmt->body.get());
    
    // Propagate any evaluation errors
    if (constEvaluator->hasErrors()) {
        for (const auto& err : constEvaluator->getErrors()) {
            addError("comptime block evaluation failed: " + err, stmt);
        }
        constEvaluator->clearErrors();
    }
}

// ============================================================================
// Struct Declaration Type Checking
// ============================================================================

void TypeChecker::checkStructDecl(StructDeclStmt* stmt) {
    // Session 13: Check if this is a generic struct
    if (!stmt->genericParams.empty()) {
        // Generic struct - register for later monomorphization
        // Don't create the type yet - it will be created when instantiated
        auto it = genericStructRegistry.find(stmt->structName);
        if (it != genericStructRegistry.end()) {
            // Already pre-registered during preRegisterFunctions — skip silently
            return;
        }
        genericStructRegistry[stmt->structName] = stmt;
        return;  // Don't register with TypeSystem yet
    }
    
    // Non-generic struct - check if already defined.
    // The pre-registration pass calls checkStructDecl early so that struct
    // types are available during function signature resolution.  When the
    // main pass revisits the same declaration, skip silently rather than
    // emitting a spurious "already defined" error.
    Type* existingStruct = typeSystem->getStructType(stmt->structName);
    if (existingStruct) {
        // Silently skip: struct was pre-registered by preRegisterFunctions.
        // A genuine duplicate definition in user code is caught by the
        // pre-pass itself (it only registers if !getStructType()), so we
        // will never reach this point for true duplicates.
        return;
    }
    
    // Note: We don't check primitiveCache because getPrimitiveType auto-creates types
    // This is fine - struct names should just avoid conflicting with known primitives
    
    // Extract and validate field information
    std::vector<StructType::Field> fields;
    for (const auto& field : stmt->fields) {
        if (field->type == ASTNode::NodeType::VAR_DECL) {
            VarDeclStmt* fieldDecl = static_cast<VarDeclStmt*>(field.get());
            
            // v0.2.35: Borrow qualifiers not allowed on struct fields
            if (fieldDecl->isBorrowImm || fieldDecl->isBorrowMut) {
                addError("Borrow qualifiers ($$i/$$m) cannot be used on struct fields — "
                         "structs cannot hold borrowed references (v1 restriction)", field.get());
                continue;
            }
            
            // Extract type name from typeNode (parser stores it there, not in typeName)
            std::string fieldTypeName;
            Type* fieldType = nullptr;
            
            if (fieldDecl->typeNode) {
                // Use resolveTypeNode to handle all type kinds (simple, array, pointer, etc.)
                fieldType = resolveTypeNode(fieldDecl->typeNode.get());
                
                if (!fieldType || fieldType->getKind() == TypeKind::ERROR) {
                    addError("Invalid type for field '" + fieldDecl->varName + 
                            "' in struct '" + stmt->structName + "'", field.get());
                    continue;
                }
                
            } else if (!fieldDecl->typeName.empty()) {
                // Fallback to legacy typeName field for backwards compatibility
                fieldTypeName = fieldDecl->typeName;
                
                // Look up field type (check struct types first to avoid auto-creating primitives)
                fieldType = typeSystem->getStructType(fieldTypeName);
                if (!fieldType) {
                    fieldType = typeSystem->getPrimitiveType(fieldTypeName);
                }
                
                if (!fieldType || fieldType->getKind() == TypeKind::ERROR) {
                    addError("Unknown field type '" + fieldTypeName + 
                            "' in struct '" + stmt->structName + "'", field.get());
                    continue;
                }
                
            } else {
                addError("Missing type information for field '" + fieldDecl->varName + 
                        "' in struct '" + stmt->structName + "'", field.get());
                continue;
            }
            
            // Add field to list (offset 0 for now, isPublic false for now)
            fields.push_back(StructType::Field(fieldDecl->varName, fieldType, 0, false));
        }
    }
    
    // Register struct type in type system with field information
    typeSystem->createStructType(stmt->structName, fields, 0, 0, false);
}

void TypeChecker::checkEnumDecl(EnumDeclStmt* stmt) {
    // v0.2.39: Create proper EnumType for type-safe enum variables
    
    // Check if enum name already exists as a type
    if (typeSystem->getStructType(stmt->enumName) || typeSystem->getEnumType(stmt->enumName)) {
        addError("Type '" + stmt->enumName + "' is already defined", stmt);
        return;
    }
    
    // Create the EnumType in the type system
    EnumType* enumType = typeSystem->createEnumType(stmt->enumName, stmt->variants);
    
    // Register each enum variant as a typed constant in the symbol table
    for (const auto& [variantName, variantValue] : stmt->variants) {
        std::string fullName = stmt->enumName + "." + variantName;
        
        if (symbolTable->isDefined(fullName)) {
            addError("Enum variant '" + variantName + "' is already defined in enum '" + 
                    stmt->enumName + "'", stmt);
            continue;
        }
        
        // Register variant with the enum type (not int64 — enables type checking)
        symbolTable->defineSymbol(fullName, SymbolKind::VARIABLE, enumType);
    }
    
    // Also register the enum name itself so it can be used as a type annotation
    // (resolveTypeNode will look it up via typeSystem->getEnumType())
}

// ============================================================================
// v0.2.41: Rules Declaration — Refinement Types
// ============================================================================

// v0.2.43: Recursively validate $.field references in rule conditions
void TypeChecker::validateRulesDollarFields(ASTNode* node, const std::string& typeName,
                                             StructType* structType, ASTNode* errorNode) {
    if (!node) return;
    
    if (node->type == ASTNode::NodeType::MEMBER_ACCESS) {
        auto* member = static_cast<MemberAccessExpr*>(node);
        // Check if the object is the $ placeholder
        if (member->object->type == ASTNode::NodeType::IDENTIFIER) {
            auto* ident = static_cast<IdentifierExpr*>(member->object.get());
            if (ident->name == "$") {
                // Validate the field against the type parameter
                if (typeName == "string") {
                    // String built-in properties and methods
                    static const std::unordered_set<std::string> validStringMembers = {
                        "length", "contains", "startsWith", "endsWith"
                    };
                    if (validStringMembers.find(member->member) == validStringMembers.end()) {
                        addError("String type has no property '" + member->member + 
                                 "' in Rules body. Valid: $.length, $.contains(), $.startsWith(), $.endsWith()", errorNode);
                    }
                }
                // v0.2.44: Array type — accept .length
                else if (typeName.length() > 2 && typeName.substr(typeName.length()-2) == "[]") {
                    if (member->member != "length") {
                        addError("Array type has no property '" + member->member + 
                                 "' in Rules body. Valid: $.length", errorNode);
                    }
                }
                else if (structType) {
                    // Struct type — check field exists
                    bool found = false;
                    const auto& fields = structType->getFields();
                    for (const auto& field : fields) {
                        if (field.name == member->member) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        std::string available;
                        for (size_t i = 0; i < fields.size(); ++i) {
                            if (i > 0) available += ", ";
                            available += fields[i].name;
                        }
                        addError("Type '" + typeName + "' has no field '" + member->member +
                                 "' in Rules body. Available fields: " + available, errorNode);
                    }
                }
            }
        }
        // v0.2.44: Also handle $.length on $[idx] (e.g., member access on indexed expression)
        // where the object is an IndexExpr
        if (member->object->type == ASTNode::NodeType::INDEX) {
            validateRulesDollarFields(member->object.get(), typeName, structType, errorNode);
        }
    }
    
    // v0.2.44: Recurse into index expressions (for $[idx] in conditions)
    if (node->type == ASTNode::NodeType::INDEX) {
        auto* indexExpr = static_cast<IndexExpr*>(node);
        validateRulesDollarFields(indexExpr->array.get(), typeName, structType, errorNode);
    }
    
    // Recurse into binary expressions
    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* binary = static_cast<BinaryExpr*>(node);
        validateRulesDollarFields(binary->left.get(), typeName, structType, errorNode);
        validateRulesDollarFields(binary->right.get(), typeName, structType, errorNode);
    }
    
    // Recurse into call expressions (for $.contains() etc.)
    if (node->type == ASTNode::NodeType::CALL) {
        auto* call = static_cast<CallExpr*>(node);
        validateRulesDollarFields(call->callee.get(), typeName, structType, errorNode);
        for (const auto& arg : call->arguments) {
            validateRulesDollarFields(arg.get(), typeName, structType, errorNode);
        }
    }
}

void TypeChecker::checkRulesDecl(RulesDeclStmt* stmt) {
    // Check for duplicate rules name
    if (rulesTable.find(stmt->rulesName) != rulesTable.end()) {
        addError("Rules '" + stmt->rulesName + "' is already defined", stmt);
        return;
    }
    
    // v0.2.42: Validate type parameters are known types
    // v0.2.43: Also accept struct/Type names for $.field member access
    // v0.2.44: Also accept array types (T[]) and pointer types (T@)
    for (const auto& typeParam : stmt->typeParams) {
        std::string baseType = typeParam;
        // v0.2.44: Strip array suffix [] or pointer suffix @ to validate the base type
        if (baseType.length() > 2 && baseType.substr(baseType.length()-2) == "[]") {
            baseType = baseType.substr(0, baseType.length()-2);
        } else if (baseType.length() > 1 && baseType.back() == '@') {
            baseType = baseType.substr(0, baseType.length()-1);
        }
        if (!isTypeKeyword(baseType) && !typeSystem->getStructType(baseType)) {
            std::string suggestion = findSimilarType(baseType);
            std::string msg = "Unknown type '" + typeParam + "' in Rules<> type parameter";
            if (!suggestion.empty()) {
                msg += ". Did you mean '" + suggestion + "'?";
            }
            addError(msg, stmt);
            return;
        }
    }
    
    // Validate cascaded rule references exist
    for (const auto& cascadedName : stmt->cascadedRules) {
        if (rulesTable.find(cascadedName) == rulesTable.end()) {
            addError("Cascaded rules '" + cascadedName + "' referenced in Rules:" + 
                     stmt->rulesName + " has not been declared yet", stmt);
            return;
        }
    }
    
    // v0.2.43: Validate $.field member access against the type parameter
    if (!stmt->typeParams.empty()) {
        const std::string& typeName = stmt->typeParams[0];
        StructType* structType = nullptr;
        Type* resolvedType = typeSystem->getStructType(typeName);
        if (resolvedType && resolvedType->getKind() == TypeKind::STRUCT) {
            structType = static_cast<StructType*>(resolvedType);
        }
        
        // Walk each condition looking for MemberAccessExpr on $
        for (const auto& cond : stmt->conditions) {
            validateRulesDollarFields(cond.get(), typeName, structType, stmt);
        }
    }
    
    // Register in rules table
    rulesTable[stmt->rulesName] = stmt;
}

// Compile-time evaluation of limit rules against an integer literal value
void TypeChecker::validateLimitRules(const std::string& rulesName, int64_t value, ASTNode* stmt) {
    auto it = rulesTable.find(rulesName);
    if (it == rulesTable.end()) return;
    
    RulesDeclStmt* rules = it->second;
    
    // Check cascaded rules first (recursively)
    for (const auto& cascadedName : rules->cascadedRules) {
        validateLimitRules(cascadedName, value, stmt);
    }
    
    // Evaluate each condition by substituting $ with the value
    for (size_t i = 0; i < rules->conditions.size(); ++i) {
        auto& condition = rules->conditions[i];
        if (!evaluateRuleConditionInt(condition.get(), value)) {
            addError("limit<" + rulesName + "> violation: value " + std::to_string(value) +
                     " fails rule condition '" + condition->toString() + "'", stmt);
        }
    }
}

// Compile-time evaluation of limit rules against a float literal value
void TypeChecker::validateLimitRulesFloat(const std::string& rulesName, double value, ASTNode* stmt) {
    auto it = rulesTable.find(rulesName);
    if (it == rulesTable.end()) return;
    
    RulesDeclStmt* rules = it->second;
    
    // Check cascaded rules first (recursively)
    for (const auto& cascadedName : rules->cascadedRules) {
        validateLimitRulesFloat(cascadedName, value, stmt);
    }
    
    // Evaluate each condition by substituting $ with the value
    for (size_t i = 0; i < rules->conditions.size(); ++i) {
        auto& condition = rules->conditions[i];
        if (!evaluateRuleConditionFloat(condition.get(), value)) {
            addError("limit<" + rulesName + "> violation: value " + std::to_string(value) +
                     " fails rule condition '" + condition->toString() + "'", stmt);
        }
    }
}

// Evaluate a single rule condition AST node with integer $ substitution
bool TypeChecker::evaluateRuleConditionInt(ASTNode* node, int64_t dollarValue) {
    if (!node) return true;
    
    // Identifier "$" → return the dollar value
    if (node->type == ASTNode::NodeType::IDENTIFIER) {
        auto* ident = static_cast<IdentifierExpr*>(node);
        if (ident->name == "$") return true; // Shouldn't happen at leaf in a binary expr
    }
    
    // Binary expression: evaluate both sides
    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* binary = static_cast<BinaryExpr*>(node);
        int64_t left = evalRuleOperandInt(binary->left.get(), dollarValue);
        int64_t right = evalRuleOperandInt(binary->right.get(), dollarValue);
        
        switch (binary->op.type) {
            case frontend::TokenType::TOKEN_LESS:          return left < right;
            case frontend::TokenType::TOKEN_LESS_EQUAL:    return left <= right;
            case frontend::TokenType::TOKEN_GREATER:       return left > right;
            case frontend::TokenType::TOKEN_GREATER_EQUAL: return left >= right;
            case frontend::TokenType::TOKEN_EQUAL_EQUAL:   return left == right;
            case frontend::TokenType::TOKEN_BANG_EQUAL:    return left != right;
            default:
                // For non-comparison operators (%, +, etc.), this is a sub-expression
                // that should have been evaluated in evalRuleOperandInt
                return true;
        }
    }
    
    return true; // Unknown node type — pass (runtime will catch it)
}

// Evaluate a rule operand (part of a condition) to an integer value
int64_t TypeChecker::evalRuleOperandInt(ASTNode* node, int64_t dollarValue) {
    if (!node) return 0;
    
    // $ placeholder
    if (node->type == ASTNode::NodeType::IDENTIFIER) {
        auto* ident = static_cast<IdentifierExpr*>(node);
        if (ident->name == "$") return dollarValue;
    }
    
    // Integer literal
    if (node->type == ASTNode::NodeType::LITERAL) {
        auto* literal = static_cast<LiteralExpr*>(node);
        if (std::holds_alternative<int64_t>(literal->value)) {
            return std::get<int64_t>(literal->value);
        }
        if (std::holds_alternative<double>(literal->value)) {
            return static_cast<int64_t>(std::get<double>(literal->value));
        }
    }
    
    // Unary minus
    if (node->type == ASTNode::NodeType::UNARY_OP) {
        auto* unary = static_cast<UnaryExpr*>(node);
        if (unary->op.type == frontend::TokenType::TOKEN_MINUS) {
            return -evalRuleOperandInt(unary->operand.get(), dollarValue);
        }
    }
    
    // Binary sub-expression (e.g., $ % 2 in "$ % 2 == 0")
    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* binary = static_cast<BinaryExpr*>(node);
        int64_t left = evalRuleOperandInt(binary->left.get(), dollarValue);
        int64_t right = evalRuleOperandInt(binary->right.get(), dollarValue);
        
        switch (binary->op.type) {
            case frontend::TokenType::TOKEN_PLUS:    return left + right;
            case frontend::TokenType::TOKEN_MINUS:   return left - right;
            case frontend::TokenType::TOKEN_STAR:    return left * right;
            case frontend::TokenType::TOKEN_SLASH:   return (right != 0) ? left / right : 0;
            case frontend::TokenType::TOKEN_PERCENT: return (right != 0) ? left % right : 0;
            default: return 0;
        }
    }
    
    return 0;
}

// Evaluate a single rule condition AST node with float $ substitution
bool TypeChecker::evaluateRuleConditionFloat(ASTNode* node, double dollarValue) {
    if (!node) return true;
    
    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* binary = static_cast<BinaryExpr*>(node);
        double left = evalRuleOperandFloat(binary->left.get(), dollarValue);
        double right = evalRuleOperandFloat(binary->right.get(), dollarValue);
        
        switch (binary->op.type) {
            case frontend::TokenType::TOKEN_LESS:          return left < right;
            case frontend::TokenType::TOKEN_LESS_EQUAL:    return left <= right;
            case frontend::TokenType::TOKEN_GREATER:       return left > right;
            case frontend::TokenType::TOKEN_GREATER_EQUAL: return left >= right;
            case frontend::TokenType::TOKEN_EQUAL_EQUAL:   return left == right;
            case frontend::TokenType::TOKEN_BANG_EQUAL:    return left != right;
            default: return true;
        }
    }
    
    return true;
}

double TypeChecker::evalRuleOperandFloat(ASTNode* node, double dollarValue) {
    if (!node) return 0.0;
    
    if (node->type == ASTNode::NodeType::IDENTIFIER) {
        auto* ident = static_cast<IdentifierExpr*>(node);
        if (ident->name == "$") return dollarValue;
    }
    
    if (node->type == ASTNode::NodeType::LITERAL) {
        auto* literal = static_cast<LiteralExpr*>(node);
        if (std::holds_alternative<double>(literal->value)) {
            return std::get<double>(literal->value);
        }
        if (std::holds_alternative<int64_t>(literal->value)) {
            return static_cast<double>(std::get<int64_t>(literal->value));
        }
    }
    
    if (node->type == ASTNode::NodeType::UNARY_OP) {
        auto* unary = static_cast<UnaryExpr*>(node);
        if (unary->op.type == frontend::TokenType::TOKEN_MINUS) {
            return -evalRuleOperandFloat(unary->operand.get(), dollarValue);
        }
    }
    
    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* binary = static_cast<BinaryExpr*>(node);
        double left = evalRuleOperandFloat(binary->left.get(), dollarValue);
        double right = evalRuleOperandFloat(binary->right.get(), dollarValue);
        
        switch (binary->op.type) {
            case frontend::TokenType::TOKEN_PLUS:    return left + right;
            case frontend::TokenType::TOKEN_MINUS:   return left - right;
            case frontend::TokenType::TOKEN_STAR:    return left * right;
            case frontend::TokenType::TOKEN_SLASH:   return (right != 0.0) ? left / right : 0.0;
            case frontend::TokenType::TOKEN_PERCENT: return (right != 0.0) ? std::fmod(left, right) : 0.0;
            default: return 0.0;
        }
    }
    
    return 0.0;
}

// ============================================================================
// Type Oriented Programming (TOP) - Type Declaration Desugaring
// ============================================================================

void TypeChecker::checkTypeDecl(TypeDeclStmt* stmt) {
    // Type Oriented Programming: Desugar Type:Name into struct + prefixed methods
    // This provides class-like organization without OOP baggage
    
    // Step 1: Check if Type name already exists
    Type* existingType = typeSystem->getStructType(stmt->typeName);
    bool preRegistered = (existingType != nullptr);
    if (preRegistered) {
        // The struct was pre-registered by the TYPE_DECL pre-pass in preRegisterFunctions
        // so that function signatures could resolve this type during the function pre-pass.
        // We still need to process and register the methods below — skip to Step 3.
        goto register_methods;
    }
    
    {
    // Step 2: Merge struct:internal + struct:interface into single struct
    std::vector<ASTNodePtr> mergedFields;
    
    if (stmt->internalStruct) {
        auto internalStruct = std::static_pointer_cast<StructDeclStmt>(stmt->internalStruct);
        for (const auto& field : internalStruct->fields) {
            mergedFields.push_back(field);
        }
    }
    
    if (stmt->interfaceStruct) {
        auto interfaceStruct = std::static_pointer_cast<StructDeclStmt>(stmt->interfaceStruct);
        for (const auto& field : interfaceStruct->fields) {
            mergedFields.push_back(field);
        }
    }
    
    // Create merged struct declaration
    auto mergedStruct = std::make_shared<StructDeclStmt>(
        stmt->typeName,
        mergedFields,
        stmt->line,
        stmt->column
    );
    
    // Process the merged struct through normal struct checking
    checkStructDecl(mergedStruct.get());
    } // end scope for Step 2 locals
    
register_methods:
    {
    // Step 3: Register Type in symbol table with the actual (now-created) struct type
    Type* structType = typeSystem->getStructType(stmt->typeName);
    symbolTable->defineSymbol(stmt->typeName, SymbolKind::TYPE, structType);
    
    // Step 4: Process func:create as TypeName_create
    if (stmt->createFunc) {
        auto createFunc = std::static_pointer_cast<FuncDeclStmt>(stmt->createFunc);
        std::string prefixedName = stmt->typeName + "_" + createFunc->funcName;
        createFunc->funcName = prefixedName;
        checkFuncDecl(createFunc.get());
    }
    
    // Step 5: Process func:destroy as TypeName_destroy
    if (stmt->destroyFunc) {
        auto destroyFunc = std::static_pointer_cast<FuncDeclStmt>(stmt->destroyFunc);
        std::string prefixedName = stmt->typeName + "_" + destroyFunc->funcName;
        destroyFunc->funcName = prefixedName;
        checkFuncDecl(destroyFunc.get());
    }
    
    // Step 6: Process all methods as TypeName_methodName
    for (const auto& method : stmt->methods) {
        auto methodFunc = std::static_pointer_cast<FuncDeclStmt>(method);
        std::string prefixedName = stmt->typeName + "_" + methodFunc->funcName;
        methodFunc->funcName = prefixedName;
        checkFuncDecl(methodFunc.get());
    }
    
    // Note: struct:type (static members) would be handled similarly to regular structs
    // For now, we skip static member struct processing
    // Type.MEMBER access is already transformed by parser to Type_MEMBER() calls
    } // end register_methods block
}

// ============================================================================
// Trait System Type Checking (WP 005)
// ============================================================================

void TypeChecker::checkTraitDecl(TraitDeclStmt* stmt) {
    // Check if trait name already exists
    if (symbolTable->isDefined(stmt->traitName)) {
        addError("Trait '" + stmt->traitName + "' is already defined", stmt);
        return;
    }

    // Register the trait in the symbol table
    Symbol* traitSymbol = symbolTable->defineSymbol(
        stmt->traitName,
        SymbolKind::TRAIT,
        nullptr  // Traits don't have a direct Type representation yet
    );

    if (traitSymbol) {
        traitSymbol->setTraitDecl(stmt);
    }

    // Validate method signatures (basic validation)
    for (const auto& method : stmt->methods) {
        // Check that return type is valid
        // For now, just log the method for debugging
        // TODO: Validate parameter types exist
    }
}

void TypeChecker::checkImplDecl(ImplDeclStmt* stmt) {
    // Look up the trait
    Symbol* traitSymbol = symbolTable->lookupSymbol(stmt->traitName);
    if (!traitSymbol || traitSymbol->kind != SymbolKind::TRAIT) {
        addError("Trait '" + stmt->traitName + "' is not defined", stmt);
        return;
    }

    // Look up the type (struct) being implemented for
    Symbol* typeSymbol = symbolTable->lookupSymbol(stmt->typeName);
    if (!typeSymbol || typeSymbol->kind != SymbolKind::TYPE) {
        // Type might be a primitive - that's ok for now
        // TODO: Better type resolution
    }

    // Get the trait declaration
    TraitDeclStmt* traitDecl = traitSymbol->getTraitDecl();
    if (!traitDecl) {
        addError("Internal error: Trait '" + stmt->traitName + "' has no declaration", stmt);
        return;
    }

    // Register this impl with the trait
    traitSymbol->addImplDecl(stmt);

    // Empty impl blocks indicate intrinsic implementation
    // Example: impl:Numeric:for:int32 = {}
    // The compiler provides the operations as intrinsics, so no methods need to be defined
    if (stmt->methods.empty()) {
        // Intrinsic implementation - all trait methods assumed to be provided by compiler
        return;
    }

    // Check that all trait methods are implemented
    std::set<std::string> implementedMethods;
    for (const auto& methodNode : stmt->methods) {
        if (auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(methodNode)) {
            implementedMethods.insert(funcDecl->funcName);

            // Register the mangled function name: TypeName_methodName
            // This allows UFCS-style calls like object.method() -> TypeName_method(object)
            std::string mangledName = stmt->typeName + "_" + funcDecl->funcName;

            // Create a function type for the mangled function
            std::vector<Type*> paramTypes;
            for (const auto& param : funcDecl->parameters) {
                if (param->type == ASTNode::NodeType::PARAMETER) {
                    ParameterNode* pnode = static_cast<ParameterNode*>(param.get());
                    if (pnode->typeNode) {
                        std::string paramTypeName = pnode->typeNode->toString();
                        // Try struct type first (covers user-defined struct params like
                        // 'Counter:self'), then primitive, then i64 fallback
                        Type* ptype = typeSystem->getStructType(paramTypeName);
                        if (!ptype) ptype = typeSystem->getPrimitiveType(paramTypeName);
                        if (!ptype) ptype = typeSystem->getPrimitiveType("int64");  // fallback
                        paramTypes.push_back(ptype);
                    }
                }
            }

            std::string retTypeName = funcDecl->returnType ? funcDecl->returnType->toString() : "void";
            // Same struct-first lookup for the return type
            Type* retType = typeSystem->getStructType(retTypeName);
            if (!retType) retType = typeSystem->getPrimitiveType(retTypeName);
            if (!retType) retType = typeSystem->getPrimitiveType("void");

            // Impl methods are regular Aria functions — wrap return in Result<T>
            // (same as checkFuncDecl does for non-extern functions)
            Type* voidT = typeSystem->getPrimitiveType("void");
            if (funcDecl->body && !retType->equals(voidT) && retType->getKind() != TypeKind::RESULT) {
                retType = typeSystem->getResultType(retType);
            }

            Type* funcType = typeSystem->getFunctionType(paramTypes, retType, false);

            Symbol* funcSymbol = symbolTable->defineSymbol(mangledName, SymbolKind::FUNCTION, funcType);
            if (funcSymbol) {
                funcSymbol->setFuncDecl(funcDecl.get());
            }

            // Type check the method implementation
            checkFuncDecl(funcDecl.get());
        }
    }

    // Verify all required methods are present
    for (const auto& requiredMethod : traitDecl->methods) {
        if (implementedMethods.find(requiredMethod.name) == implementedMethods.end()) {
            addError("Missing implementation of method '" + requiredMethod.name +
                    "' from trait '" + stmt->traitName + "' for type '" + stmt->typeName + "'", stmt);
        }
    }

    // TODO: Verify method signatures match
    // This requires comparing parameter types and return types
}

// ============================================================================
// Assignment Type Checking
// ============================================================================

void TypeChecker::checkAssignment(BinaryExpr* expr) {
    // Special case: Explicit discard with _
    if (expr->left->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr->left.get());
        if (ident->name == "_") {
            // Discard - just type check the right side, don't create symbol
            inferType(expr->right.get());
            return;
        }
    }
    
    // ========================================================================
    // Phase 1.5: Immutable .is_error Enforcement
    // ========================================================================
    // Block assignment to Result<T>.is_error unless variable has 'wild' modifier
    // Prevents error state corruption - errors are facts that must propagate
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
                    return;
                }
            }
        }
    }
    
    // Left side must be an lvalue (identifier, index, member access, or dereference)
    bool isValidLvalue = false;
    
    if (expr->left->type == ASTNode::NodeType::IDENTIFIER ||
        expr->left->type == ASTNode::NodeType::INDEX ||
        expr->left->type == ASTNode::NodeType::MEMBER_ACCESS ||
        expr->left->type == ASTNode::NodeType::POINTER_MEMBER) {
        isValidLvalue = true;
    } else if (expr->left->type == ASTNode::NodeType::UNARY_OP) {
        // Allow dereference operators (<- and *) on left side
        // Example: <-ptr = 42; assigns through the pointer
        UnaryExpr* unary = static_cast<UnaryExpr*>(expr->left.get());
        if (unary->op.type == TokenType::TOKEN_LEFT_ARROW || 
            unary->op.type == TokenType::TOKEN_STAR) {
            isValidLvalue = true;
        }
    }
    
    if (!isValidLvalue) {
        addError("Left side of assignment must be a variable, array element, member, or pointer dereference", expr->left.get());
        return;
    }
    
    // Get left side type
    Type* leftType = inferType(expr->left.get());
    if (leftType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // Check if assigning to const variable
    if (expr->left->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr->left.get());
        Symbol* symbol = symbolTable->lookupSymbol(ident->name);
        
        if (symbol && symbol->kind == SymbolKind::CONSTANT) {
            addError("Cannot assign to const variable '" + ident->name + "'", expr);
            return;
        }
        
        // Check if assigning to fixed variable
        if (symbol && symbol->isFixed) {
            addError("Cannot reassign fixed variable '" + ident->name + "' - fixed variables are immutable", expr);
            return;
        }
        
        // Check if assigning through immutable borrow ($$i)
        if (symbol && symbol->isBorrowImm) {
            addError("Cannot assign through immutable borrow '" + ident->name + 
                     "' — use $$m for a mutable borrow", expr);
            return;
        }
    }
    
    // Check if modifying field of fixed variable
    if (expr->left->type == ASTNode::NodeType::MEMBER_ACCESS) {
        MemberAccessExpr* member = static_cast<MemberAccessExpr*>(expr->left.get());
        
        // Find the base identifier
        ASTNode* base = member->object.get();
        while (base->type == ASTNode::NodeType::MEMBER_ACCESS) {
            MemberAccessExpr* baseMember = static_cast<MemberAccessExpr*>(base);
            base = baseMember->object.get();
        }
        
        if (base->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* ident = static_cast<IdentifierExpr*>(base);
            Symbol* symbol = symbolTable->lookupSymbol(ident->name);
            
            if (symbol && symbol->isFixed) {
                addError("Cannot modify field of fixed variable '" + ident->name + "' - fixed variables are immutable", expr);
                return;
            }
        }
    }
    
    // Get right side type
    Type* rightType = inferType(expr->right.get());
    if (rightType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // Standard Integer Literal Assignment (Phase 4.5.4)
    // Allow safe narrowing for integer literals in assignments (e.g., x = 5)
    // This makes direct assignments consistent with variable declarations
    bool standardIntLiteralAssignment = false;
    if (isStandardIntType(leftType) && expr->right->type == ASTNode::NodeType::LITERAL) {
        LiteralExpr* literal = static_cast<LiteralExpr*>(expr->right.get());
        if (std::holds_alternative<int64_t>(literal->value)) {
            int64_t value = std::get<int64_t>(literal->value);
            if (literalFitsInType(value, leftType)) {
                // Literal fits, narrow the type to allow assignment
                rightType = leftType;
                standardIntLiteralAssignment = true;
            } else {
                // Literal doesn't fit, report error
                canLiteralFitInIntType(value, leftType, expr);
                return;
            }
        }
    }
    
    // TBB Type Validation (Phase 3.2.4)
    // Special handling for integer literals assigned to TBB types
    bool tbbLiteralAssignment = false;
    if (isTBBType(leftType) && expr->right->type == ASTNode::NodeType::LITERAL) {
        LiteralExpr* literal = static_cast<LiteralExpr*>(expr->right.get());
        if (std::holds_alternative<int64_t>(literal->value)) {
            int64_t value = std::get<int64_t>(literal->value);
            checkTBBLiteralValue(value, leftType, expr);
            tbbLiteralAssignment = true;
            
            // If validation failed, early exit
            // If succeeded, allow the assignment (literal is in range)
        }
    }
    
    // Balanced Type Validation (Phase 3.2.5)
    // Special handling for integer literals assigned to balanced types
    bool balancedLiteralAssignment = false;
    if (isBalancedType(leftType) && expr->right->type == ASTNode::NodeType::LITERAL) {
        LiteralExpr* literal = static_cast<LiteralExpr*>(expr->right.get());
        if (std::holds_alternative<int64_t>(literal->value)) {
            int64_t value = std::get<int64_t>(literal->value);
            checkBalancedLiteralValue(value, leftType, expr);
            balancedLiteralAssignment = true;
            
            // If validation failed, early exit
            // If succeeded, allow the assignment (literal is in range)
        }
    }
    
    // Check type compatibility (skip if we handled standard int, TBB, or balanced literal assignment)
    if (!standardIntLiteralAssignment && !tbbLiteralAssignment && !balancedLiteralAssignment) {
        // Special case: Allow T to be assigned to T? (wrapping)
        bool optionalWrapping = false;
        if (leftType->getKind() == TypeKind::OPTIONAL) {
            const OptionalType* leftOptional = static_cast<const OptionalType*>(leftType);
            if (rightType->isAssignableTo(leftOptional->getWrappedType())) {
                optionalWrapping = true;
            }
        }
        
        if (!rightType->isAssignableTo(leftType) && !canCoerce(rightType, leftType) && !optionalWrapping) {
            addError("Cannot assign value of type '" + rightType->toString() + 
                    "' to variable of type '" + leftType->toString() + "'", expr);
        }
    }
}

// ============================================================================
// Return Statement Type Checking
// ============================================================================

void TypeChecker::checkReturnStmt(ReturnStmt* stmt) {
    // Check if we're inside a function
    if (!currentFunctionReturnType) {
        addError("return statement outside of function", stmt);
        return;
    }
    
    // Get the primitive type "void" for comparison
    Type* voidType = typeSystem->getPrimitiveType("void");
    bool isVoidFunction = currentFunctionReturnType->equals(voidType);
    
    // Case 1: void function with return value
    if (isVoidFunction && stmt->value) {
        addError("NIL function cannot return a value. Use 'return;' or omit the return statement", stmt);
        return;
    }
    
    // Case 2: non-void function without return value
    if (!isVoidFunction && !stmt->value) {
        addError("Function must return a value of type '" + 
                currentFunctionReturnType->toString() + "'", stmt);
        return;
    }
    
    // Case 3: non-void function with return value - check type
    if (!isVoidFunction && stmt->value) {
        Type* returnType = inferType(stmt->value.get());
        
        if (returnType->getKind() == TypeKind::ERROR) {
            return;
        }
        
        // Context-aware literal typing for return statements
        // If returning an int64 literal to a smaller int type, check if it fits
        if (stmt->value->type == ASTNode::NodeType::LITERAL &&
            isStandardIntType(currentFunctionValueType) &&
            returnType->toString() == "int64") {
            
            LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->value.get());
            if (std::holds_alternative<int64_t>(literal->value)) {
                int64_t value = std::get<int64_t>(literal->value);
                if (literalFitsInType(value, currentFunctionValueType)) {
                    // Treat literal as having the function's value type
                    return;  // Success, literal fits
                }
            }
        }
        
        // Return statements should check against the VALUE type, not the Result<T> wrapper
        // Functions internally return Result<T>, but the return statement provides the value.
        // Auto-unwrap Result<T> when the caller returns a direct call to another Aria function.
        Type* effectiveReturnType = returnType;
        if (returnType->getKind() == TypeKind::RESULT) {
            ResultType* resultWrapper = static_cast<ResultType*>(returnType);
            effectiveReturnType = resultWrapper->getValueType();
        }
        if (!effectiveReturnType->isAssignableTo(currentFunctionValueType) && 
            !canCoerce(effectiveReturnType, currentFunctionValueType)) {
            addError("Return type '" + returnType->toString() + 
                    "' does not match function return type '" + 
                    currentFunctionValueType->toString() + "'", stmt);
        }
    }
}

// ============================================================================
// Pass/Fail Statement Type Checking (Result Types)
// ============================================================================

void TypeChecker::checkPassStmt(PassStmt* stmt) {
    // Check if we're inside a function
    if (!currentFunctionValueType) {
        addError("pass statement outside of function", stmt);
        return;
    }

    // pass() is not allowed in main or failsafe — use exit() instead
    if (currentFunctionName == "main" || currentFunctionName == "failsafe") {
        addError("pass() cannot be used in '" + currentFunctionName + "'. "
                 "Use exit(code) to terminate program endpoints.", stmt);
        return;
    }
    
    // Get the primitive type "NIL" for comparison
    Type* nilType = typeSystem->getPrimitiveType("NIL");
    bool isNilFunction = currentFunctionValueType->equals(nilType);
    
    // Infer type of the pass value
    Type* valueType = inferType(stmt->value.get());
    
    if (valueType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // pass(value) builds Result{val: value, err: NULL}
    // Validate that value type matches the function's declared value type
    // func:foo = int32(...) { pass(42i32); } - int32 must match int32
    // func:bar = NIL(...) { pass(NIL); } - NIL must match NIL
    
    // Special case: Allow integer literals for exotic types (same as variable init)
    bool exoticLiteralPass = false;
    if ((isBalancedType(currentFunctionValueType) || isNumericExoticType(currentFunctionValueType) || isTBBType(currentFunctionValueType)) &&
        (valueType->toString() == "int32" || valueType->toString() == "int64")) {
        // Allow int32/int64 to be passed to exotic types (will be converted in IR gen)
        exoticLiteralPass = true;
    }
    
    // Special case: Allow integer literals for optional integer types
    bool optionalIntLiteralPass = false;
    if (currentFunctionValueType->getKind() == TypeKind::OPTIONAL) {
        const OptionalType* optType = static_cast<const OptionalType*>(currentFunctionValueType);
        Type* wrappedType = optType->getWrappedType();
        if (isStandardIntType(wrappedType) && 
            (valueType->toString() == "int32" || valueType->toString() == "int64")) {
            // Check if the literal value fits in the target type
            if (stmt->value->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->value.get());
                if (std::holds_alternative<int64_t>(literal->value)) {
                    int64_t value = std::get<int64_t>(literal->value);
                    if (literalFitsInType(value, wrappedType)) {
                        optionalIntLiteralPass = true;
                    }
                }
            }
        }
    }
    
    // Auto-unwrap Result<T> when the caller passes a direct call to another Aria function.
    Type* effectiveValueType = valueType;
    if (valueType->getKind() == TypeKind::RESULT) {
        ResultType* resultWrapper = static_cast<ResultType*>(valueType);
        effectiveValueType = resultWrapper->getValueType();
    }
    if (!exoticLiteralPass && !optionalIntLiteralPass && 
        !effectiveValueType->isAssignableTo(currentFunctionValueType) && 
        !canCoerce(effectiveValueType, currentFunctionValueType)) {
        addError("Pass value type '" + valueType->toString() + 
                "' does not match function value type '" + 
                currentFunctionValueType->toString() + "'", stmt);
    }
}

void TypeChecker::checkFailStmt(FailStmt* stmt) {
    // Check if we're inside a function
    if (!currentFunctionReturnType) {
        addError("fail statement outside of function", stmt);
        return;
    }
    
    // fail() is not allowed in main or failsafe — use exit() instead
    if (currentFunctionName == "main" || currentFunctionName == "failsafe") {
        addError("fail() cannot be used in '" + currentFunctionName + "'. "
                 "Use exit(code) to terminate program endpoints.", stmt);
        return;
    }
    
    // Infer type of the error code
    Type* errorType = inferType(stmt->errorCode.get());
    
    if (errorType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // fail(err) builds Result{val: NULL, err: err}
    // TODO: Validate error type matches TBB (to-be-built) error type system
    // For now, ensure error code is an integer type
    if (errorType->getKind() != TypeKind::PRIMITIVE) {
        addError("Fail error code must be an integer type", stmt);
        return;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(errorType);
    if (!isStandardIntType(primType)) {
        addError("Fail error code must be an integer type, got '" + errorType->toString() + "'", stmt);
    }
}


// ============================================================================
// If Statement Type Checking
// ============================================================================

void TypeChecker::checkIfStmt(IfStmt* stmt) {
    // Check condition type
    Type* condType = inferType(stmt->condition.get());
    
    if (condType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // Condition must be bool (strict, no truthiness)
    PrimitiveType* condPrim = dynamic_cast<PrimitiveType*>(condType);
    if (!condPrim || condPrim->getName() != "bool") {
        addError("if condition must be 'bool' type, got '" + condType->toString() + 
                "'. Use explicit comparison (e.g., x != 0) instead of truthiness.", stmt->condition.get());
    }
    
    // Phase 2: Control flow analysis for Result checking
    // Save current state before branching
    ResultCheckState beforeState = currentResultState;
    
    // Analyze condition to extract Result check knowledge
    ResultCheckState thenState = beforeState;
    ResultCheckState elseState = beforeState;
    analyzeConditionForResultChecks(stmt->condition.get(), thenState, elseState);
    
    // Check then branch with then knowledge
    currentResultState = thenState;
    checkStatement(stmt->thenBranch.get());
    ResultCheckState afterThen = currentResultState;
    
    // Check else branch with else knowledge (if present)
    ResultCheckState afterElse;
    if (stmt->elseBranch) {
        currentResultState = elseState;
        checkStatement(stmt->elseBranch.get());
        afterElse = currentResultState;
    } else {
        // No else branch means execution can skip the if entirely
        afterElse = elseState;
    }
    
    // Early return analysis: if one branch always returns, use other branch's state
    bool thenReturns = branchAlwaysReturns(stmt->thenBranch.get());
    bool elseReturns = stmt->elseBranch && branchAlwaysReturns(stmt->elseBranch.get());
    
    if (thenReturns && !elseReturns) {
        // if (err) { return; } → after knows !err
        currentResultState = afterElse;
    } else if (elseReturns && !thenReturns) {
        // if (!err) { return; } → after knows err
        currentResultState = afterThen;
    } else if (thenReturns && elseReturns) {
        // Both branches diverge, execution doesn't continue
        // Keep merged state anyway for safety
        currentResultState = ResultCheckState::merge(afterThen, afterElse);
    } else {
        // Normal case: merge both branches conservatively
        currentResultState = ResultCheckState::merge(afterThen, afterElse);
    }
}

// ============================================================================
// While Statement Type Checking
// ============================================================================

void TypeChecker::checkWhileStmt(WhileStmt* stmt) {
    // Check condition type
    Type* condType = inferType(stmt->condition.get());
    
    if (condType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // Condition must be bool (strict, no truthiness)
    PrimitiveType* condPrim = dynamic_cast<PrimitiveType*>(condType);
    if (!condPrim || condPrim->getName() != "bool") {
        addError("while condition must be 'bool' type, got '" + condType->toString() + 
                "'. Use explicit comparison (e.g., x != 0) instead of truthiness.", stmt->condition.get());
    }
    
    // Phase 3: Control flow analysis for loops
    // Save current state before loop
    ResultCheckState beforeLoop = currentResultState;
    
    // Analyze condition to extract Result check knowledge
    ResultCheckState inLoop = beforeLoop;
    ResultCheckState afterLoop = beforeLoop;
    analyzeConditionForResultChecks(stmt->condition.get(), inLoop, afterLoop);
    
    // Check body with loop condition knowledge
    // (e.g., if while(!r.is_error), body knows KNOWN_SUCCESS)
    currentResultState = inLoop;
    checkStatement(stmt->body.get());
    
    // After loop: condition is FALSE (that's why we exited)
    // So use the 'else' state from condition analysis
    // But also be conservative: loop might not have executed at all (0 iterations)
    // Merge: what we knew before loop OR what condition being false proves
    currentResultState = ResultCheckState::merge(beforeLoop, afterLoop);
}

// ============================================================================
// For Statement Type Checking
// ============================================================================

void TypeChecker::checkForStmt(ForStmt* stmt) {
    if (stmt->isRangeBased) {
        // Range-based for loop: for (var in range) { body }
        // Check the range expression
        inferType(stmt->rangeExpr.get());
        // Range expressions currently always return a range struct, no need to validate further
        
        // Enter new scope for the loop body
        symbolTable->enterScope(ScopeKind::BLOCK, "forrange");
        
        // Add iterator variable to symbol table
        // If iteratorType is specified, use it; otherwise default to int64
        std::string iterType = stmt->iteratorType.empty() ? "int64" : stmt->iteratorType;
        Type* varType = typeSystem->getPrimitiveType(iterType);
        if (!varType || varType->getKind() == TypeKind::ERROR) {
            // Try as struct type
            varType = typeSystem->getStructType(iterType);
            if (!varType || varType->getKind() == TypeKind::ERROR) {
                addError("Unknown type for iterator: '" + iterType + "'", stmt);
                symbolTable->exitScope();
                return;
            }
        }
        
        // Define iterator as a variable in the loop scope
        symbolTable->defineSymbol(stmt->iteratorName, SymbolKind::VARIABLE, varType, 
                                  stmt->line, stmt->column);
        
        // Phase 3: Range-based loops are conservative
        // Save state before loop and reset after (can't track through iterations)
        ResultCheckState beforeLoop = currentResultState;
        
        // Check body with iterator in scope
        checkStatement(stmt->body.get());
        
        // Conservative: reset to state before loop (don't know how many iterations)
        currentResultState = beforeLoop;
        
        // Exit scope
        symbolTable->exitScope();
        return;
    }
    
    // C-style for loop
    // Phase 3: Save state before loop
    ResultCheckState beforeLoop = currentResultState;
    
    // Check initializer if present
    if (stmt->initializer) {
        checkStatement(stmt->initializer.get());
    }
    
    // Analyze condition if present
    ResultCheckState inLoop = currentResultState;
    ResultCheckState afterLoop = currentResultState;
    
    if (stmt->condition) {
        Type* condType = inferType(stmt->condition.get());
        
        if (condType->getKind() != TypeKind::ERROR) {
            PrimitiveType* condPrim = dynamic_cast<PrimitiveType*>(condType);
            if (!condPrim || condPrim->getName() != "bool") {
                addError("for condition must be 'bool' type, got '" + condType->toString() + 
                        "'. Use explicit comparison (e.g., i < 10) instead of truthiness.", stmt->condition.get());
            }
        }
        
        // Analyze condition for Result checks (like while loops)
        analyzeConditionForResultChecks(stmt->condition.get(), inLoop, afterLoop);
    }
    
    // Check body with loop condition knowledge
    currentResultState = inLoop;
    
    // Check update if present (just infer type, any expression is valid)
    if (stmt->update) {
        inferType(stmt->update.get());
    }
    
    // Check body
    checkStatement(stmt->body.get());
    
    // After loop: merge conservative (might not have executed) with exit condition
    currentResultState = ResultCheckState::merge(beforeLoop, afterLoop);
}

// ============================================================================
// Block Statement Type Checking
// ============================================================================

void TypeChecker::checkBlockStmt(BlockStmt* stmt) {
    // Enter new scope
    symbolTable->enterScope(ScopeKind::BLOCK, "block");
    
    // Check all statements in block
    for (const auto& statement : stmt->statements) {
        checkStatement(statement.get());
    }
    
    // Exit scope
    symbolTable->exitScope();
}

// ============================================================================
// Expression Statement Type Checking
// ============================================================================

void TypeChecker::checkExpressionStmt(ExpressionStmt* stmt) {
    // Special case: check if it's an assignment (including explicit discard)
    if (stmt->expression->type == ASTNode::NodeType::BINARY_OP) {
        BinaryExpr* binExpr = static_cast<BinaryExpr*>(stmt->expression.get());
        if (binExpr->op.type == frontend::TokenType::TOKEN_EQUAL) {
            // Check for explicit discard syntax: _ = expr
            if (binExpr->left->type == ASTNode::NodeType::IDENTIFIER) {
                IdentifierExpr* ident = static_cast<IdentifierExpr*>(binExpr->left.get());
                if (ident->name == "_") {
                    // Explicit discard - infer type but skip nodiscard check
                    inferType(binExpr->right.get());
                    return;
                }
            }
            
            // Regular assignment
            checkAssignment(binExpr);
            return;
        }
    }
    
    // Otherwise just infer type (to check for errors)
    Type* exprType = inferType(stmt->expression.get());
    
    // Phase 2.1: Must-Use Result Analysis
    // Check if expression has nodiscard type and value is unused
    if (exprType && exprType->isNodiscard()) {
        addError("Unused result value. This function returns a result that must be checked.",
              stmt->expression.get());
    }
}

// ============================================================================
// TBB Type Validation - Phase 3.2.4
// ============================================================================

bool TypeChecker::isTBBType(Type* type) {
    if (!type || type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& name = primType->getName();
    
    return name == "tbb8" || name == "tbb16" || name == "tbb32" || name == "tbb64";
}

int64_t TypeChecker::getTBBErrorSentinel(Type* type) {
    if (!isTBBType(type)) {
        return 0;  // Not a TBB type
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& name = primType->getName();
    
    if (name == "tbb8") {
        return -128;  // 0x80
    } else if (name == "tbb16") {
        return -32768;  // 0x8000
    } else if (name == "tbb32") {
        return -2147483648LL;  // 0x80000000
    } else if (name == "tbb64") {
        return INT64_MIN;  // 0x8000000000000000
    }
    
    return 0;
}

std::pair<int64_t, int64_t> TypeChecker::getTBBValidRange(Type* type) {
    if (!isTBBType(type)) {
        return {0, 0};  // Not a TBB type
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& name = primType->getName();
    
    if (name == "tbb8") {
        return {-127, 127};
    } else if (name == "tbb16") {
        return {-32767, 32767};
    } else if (name == "tbb32") {
        return {-2147483647LL, 2147483647LL};
    } else if (name == "tbb64") {
        return {-9223372036854775807LL, 9223372036854775807LL};
    }
    
    return {0, 0};
}

void TypeChecker::checkTBBLiteralValue(int64_t value, Type* type, ASTNode* node) {
    if (!isTBBType(type)) {
        return;  // Not a TBB type, no validation needed
    }
    
    int64_t errSentinel = getTBBErrorSentinel(type);
    
    // Check if value is the ERR sentinel
    if (value == errSentinel) {
        PrimitiveType* primType = static_cast<PrimitiveType*>(type);
        addError("Warning: Assigning ERR sentinel value (" + std::to_string(errSentinel) + 
                ") to " + primType->getName() + ". Use 'ERR' keyword for clarity.", node);
    }
    
    // Check if value is in valid range
    auto [minVal, maxVal] = getTBBValidRange(type);
    if (value < minVal || value > maxVal) {
        // Value is out of range (excluding ERR sentinel check above)
        if (value != errSentinel) {
            PrimitiveType* primType = static_cast<PrimitiveType*>(type);
            addError("Value " + std::to_string(value) + " is out of range for " + 
                    primType->getName() + " (valid range: [" + std::to_string(minVal) + 
                    ", " + std::to_string(maxVal) + "])", node);
        }
    }
}

bool TypeChecker::isERRProducingOperation(Type* resultType, Type* leftType, Type* rightType) {
    // If any operand is a TBB type, the result inherits TBB semantics
    // In actual runtime, ERR values propagate through operations
    // For type checking, we just verify the types are compatible
    
    // Note: This method is primarily for documentation and future expansion
    // The actual ERR propagation happens at runtime
    // Type checker just ensures TBB types are used correctly
    
    if (!isTBBType(resultType)) {
        return false;
    }
    
    // TBB + TBB = TBB (ERR propagates at runtime)
    // TBB + standard int = ERROR (mixing not allowed)
    if (isTBBType(leftType) != isTBBType(rightType)) {
        return true;  // Type mismatch will be caught by type checking
    }
    
    return false;
}

// ============================================================================
// Balanced Ternary/Nonary Type Validation - Phase 3.2.5
// ============================================================================

bool TypeChecker::isBalancedType(Type* type) {
    if (!type || type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& name = primType->getName();
    
    return name == "trit" || name == "tryte" || name == "nit" || name == "nyte";
}

bool TypeChecker::isTritType(Type* type) {
    if (!type || type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    return primType->getName() == "trit";
}

bool TypeChecker::isTryteType(Type* type) {
    if (!type || type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    return primType->getName() == "tryte";
}

bool TypeChecker::isNitType(Type* type) {
    if (!type || type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    return primType->getName() == "nit";
}

bool TypeChecker::isNyteType(Type* type) {
    if (!type || type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    return primType->getName() == "nyte";
}

std::vector<int> TypeChecker::getBalancedValidDigits(Type* type) {
    if (isTritType(type)) {
        // trit: balanced ternary digit {-1, 0, 1}
        return {-1, 0, 1};
    } else if (isNitType(type)) {
        // nit: balanced nonary digit {-4, -3, -2, -1, 0, 1, 2, 3, 4}
        return {-4, -3, -2, -1, 0, 1, 2, 3, 4};
    }
    
    // Composite types (tryte/nyte) don't have explicit digit sets
    return {};
}

bool TypeChecker::isNumericExoticType(Type* type) {
    if (!type || type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& name = primType->getName();
    
    // Fractional types (exact rational arithmetic)
    if (name == "frac8" || name == "frac16" || name == "frac32" || name == "frac64") {
        return true;
    }
    
    // Twisted floating point types
    if (name == "tfp32" || name == "tfp64") {
        return true;
    }
    
    // 9D toroidal types
    if (name == "vec9" || name == "dvec9" || name == "tmatrix" || name == "ttensor") {
        return true;
    }
    
    return false;
}

// P1-5: Helper to check if a type is numeric (for dimensional analysis)
bool TypeChecker::isNumericType(Type* type) {
    if (!type) {
        return false;
    }
    
    // Check for primitive numeric types
    if (type->getKind() == TypeKind::PRIMITIVE) {
        PrimitiveType* primType = static_cast<PrimitiveType*>(type);
        const std::string& name = primType->getName();
        
        // Standard integers (both canonical and alias forms)
        // Canonical: int8, int16, int32, int64, uint8, uint16, uint32, uint64
        // Alias: Same names supported
        if (name.find("int") == 0 || name.find("uint") == 0) {
            return true;
        }
        
        // Standard floats (both canonical and alias forms)
        // Canonical: flt32, flt64, flt128, flt256, flt512
        // Alias: float32, float64, float128, float256, float512
        if (name.find("flt") == 0 || name.find("float") == 0) {
            return true;
        }
        
        // Fixed-point types
        if (name.find("fix") == 0) {
            return true;
        }
        
        // TBB types
        if (name.find("tbb") == 0) {
            return true;
        }
        
        // Exotic numeric types
        if (isNumericExoticType(type)) {
            return true;
        }
        
        // Balanced ternary/nonary types
        if (name == "trit" || name == "tryte" || name == "nit" || name == "nyte") {
            return true;
        }
        
        // Vector types
        if (name == "vec9" || name == "dvec9") {
            return true;
        }
    }
    
    // Check for struct types that represent numeric types (like custom fixed-point)
    if (type->getKind() == TypeKind::STRUCT) {
        StructType* structType = static_cast<StructType*>(type);
        const std::string& name = structType->getName();
        
        // Fixed-point types (if defined as structs)
        if (name.find("fix") == 0) {
            return true;
        }
    }
    
    return false;
}

std::pair<int64_t, int64_t> TypeChecker::getBalancedCompositeRange(Type* type) {
    if (isTryteType(type) || isNyteType(type)) {
        // Both tryte and nyte have the same range
        // tryte: 10 trits = 3^10 = 59,049 values
        // nyte: 5 nits = 9^5 = 59,049 values
        // Range: [-29524, +29524]
        return {-29524, 29524};
    }
    
    // Atomic types (trit/nit) use getBalancedValidDigits instead
    return {0, 0};
}

void TypeChecker::checkBalancedLiteralValue(int64_t value, Type* type, ASTNode* node) {
    if (!isBalancedType(type)) {
        return;  // Not a balanced type, no validation needed
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& typeName = primType->getName();
    
    // Check atomic types (trit, nit)
    if (isTritType(type)) {
        // trit must be exactly -1, 0, 1, or -128 (TRIT_ERR sentinel)
        if (value != -1 && value != 0 && value != 1 && value != -128) {
            addError("trit value must be -1, 0, 1, or -128 (ERR), got " + std::to_string(value), node);
        }
    } else if (isNitType(type)) {
        // nit must be -4 to +4, or -128 (NIT_ERR sentinel)
        if ((value < -4 || value > 4) && value != -128) {
            addError("nit value must be in range [-4, 4] or -128 (ERR), got " + std::to_string(value), node);
        }
    } else if (isTryteType(type)) {
        // tryte: composite type, check range
        auto [minVal, maxVal] = getBalancedCompositeRange(type);
        if (value < minVal || value > maxVal) {
            addError("tryte value must be in range [" + std::to_string(minVal) + 
                    ", " + std::to_string(maxVal) + "], got " + std::to_string(value), node);
        }
    } else if (isNyteType(type)) {
        // nyte: composite type, check range
        auto [minVal, maxVal] = getBalancedCompositeRange(type);
        if (value < minVal || value > maxVal) {
            addError("nyte value must be in range [" + std::to_string(minVal) + 
                    ", " + std::to_string(maxVal) + "], got " + std::to_string(value), node);
        }
    }
}

// ============================================================================
// Standard Integer Type Validation
// ============================================================================

bool TypeChecker::isStandardIntType(Type* type) const {
    if (type->getKind() != TypeKind::PRIMITIVE) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& name = primType->getName();
    
    return name == "int1" || name == "int2" || name == "int4" ||
           name == "int8" || name == "int16" || name == "int32" || name == "int64" ||
           name == "uint1" || name == "uint2" || name == "uint4" ||
           name == "uint8" || name == "uint16" || name == "uint32" || name == "uint64";
}

bool TypeChecker::literalFitsInType(int64_t value, Type* type) const {
    if (!isStandardIntType(type)) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& typeName = primType->getName();
    
    // Determine range based on type name
    if (typeName == "int1") {
        return value >= -1 && value <= 0;
    } else if (typeName == "int2") {
        return value >= -2 && value <= 1;
    } else if (typeName == "int4") {
        return value >= -8 && value <= 7;
    } else if (typeName == "int8") {
        return value >= -128 && value <= 127;
    } else if (typeName == "int16") {
        return value >= -32768 && value <= 32767;
    } else if (typeName == "int32") {
        return value >= -2147483648LL && value <= 2147483647LL;
    } else if (typeName == "int64") {
        return true;  // int64 can hold any int64_t value
    } else if (typeName == "uint1") {
        return value >= 0 && value <= 1;
    } else if (typeName == "uint2") {
        return value >= 0 && value <= 3;
    } else if (typeName == "uint4") {
        return value >= 0 && value <= 15;
    } else if (typeName == "uint8") {
        return value >= 0 && value <= 255;
    } else if (typeName == "uint16") {
        return value >= 0 && value <= 65535;
    } else if (typeName == "uint32") {
        return value >= 0 && value <= 4294967295LL;
    } else if (typeName == "uint64") {
        return true;  // Any int64_t bit pattern is a valid uint64 value.
                      // Values >= 2^63 are stored as negative int64_t (bit-reinterp from stoull).
    }
    
    return false;  // Unknown type
}

bool TypeChecker::canLiteralFitInIntType(int64_t value, Type* type, ASTNode* node) {
    if (!isStandardIntType(type)) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& typeName = primType->getName();
    
    int64_t minVal, maxVal;
    
    // Determine range based on type name
    if (typeName == "int1") {
        minVal = -1;
        maxVal = 0;
    } else if (typeName == "int2") {
        minVal = -2;
        maxVal = 1;
    } else if (typeName == "int4") {
        minVal = -8;
        maxVal = 7;
    } else if (typeName == "int8") {
        minVal = -128;
        maxVal = 127;
    } else if (typeName == "int16") {
        minVal = -32768;
        maxVal = 32767;
    } else if (typeName == "int32") {
        minVal = -2147483648LL;
        maxVal = 2147483647LL;
    } else if (typeName == "int64") {
        // int64 can hold any int64_t value
        return true;
    } else if (typeName == "int128" || typeName == "int256" || 
               typeName == "int512" || typeName == "int1024" ||
               typeName == "int2048" || typeName == "int4096") {
        // Big integer types: if it fits in int64, it fits in these
        return true;
    } else if (typeName == "uint1") {
        if (value < 0) {
            addError("Cannot assign negative value " + std::to_string(value) + " to unsigned type " + typeName, node);
            return false;
        }
        minVal = 0;
        maxVal = 1;
    } else if (typeName == "uint2") {
        if (value < 0) {
            addError("Cannot assign negative value " + std::to_string(value) + " to unsigned type " + typeName, node);
            return false;
        }
        minVal = 0;
        maxVal = 3;
    } else if (typeName == "uint4") {
        if (value < 0) {
            addError("Cannot assign negative value " + std::to_string(value) + " to unsigned type " + typeName, node);
            return false;
        }
        minVal = 0;
        maxVal = 15;
    } else if (typeName == "uint8") {
        if (value < 0) {
            addError("Cannot assign negative value " + std::to_string(value) + " to unsigned type " + typeName, node);
            return false;
        }
        minVal = 0;
        maxVal = 255;
    } else if (typeName == "uint16") {
        if (value < 0) {
            addError("Cannot assign negative value " + std::to_string(value) + " to unsigned type " + typeName, node);
            return false;
        }
        minVal = 0;
        maxVal = 65535;
    } else if (typeName == "uint32") {
        if (value < 0) {
            addError("Cannot assign negative value " + std::to_string(value) + " to unsigned type " + typeName, node);
            return false;
        }
        minVal = 0;
        maxVal = 4294967295LL;
    } else if (typeName == "uint64") {
        // Any int64_t bit pattern is a valid uint64 value.
        // Values >= 2^63 are stored as negative int64_t after stoull bit-reinterpretation
        // in the lexer (Bug #21 fix). Rejecting negative stored values here would wrongly
        // block legal uint64 literals like 9223372036854775808u64.
        return true;
    } else if (typeName == "uint128" || typeName == "uint256" || 
               typeName == "uint512" || typeName == "uint1024" ||
               typeName == "uint2048" || typeName == "uint4096") {
        if (value < 0) {
            addError("Cannot assign negative value " + std::to_string(value) + " to unsigned type " + typeName, node);
            return false;
        }
        // Big unsigned types: if it's non-negative and fits in int64, it fits
        return true;
    } else {
        return false;  // Unknown type
    }
    
    // Check if value is in range
    if (value < minVal || value > maxVal) {
        addError("Value " + std::to_string(value) + " is out of range for " + 
                typeName + " (valid range: [" + std::to_string(minVal) + 
                ", " + std::to_string(maxVal) + "])", node);
        return false;
    }
    
    return true;
}

// ============================================================================
// Module System Type Checking
// ============================================================================

void TypeChecker::checkUseStmt(UseStmt* stmt) {
    if (!stmt) {
        return;
    }
    
    // Basic validation of use statement structure
    if (stmt->path.empty()) {
        addError("Empty module path in use statement", stmt);
        return;
    }
    
    // Validate selective imports and wildcards
    if (stmt->isWildcard && !stmt->items.empty()) {
        addError("Cannot have both wildcard (*) and selective imports in use statement", stmt);
        return;
    }
    
    // If no module loader is available, skip module loading (used in tests)
    if (!moduleLoader) {
        return;
    }
    
    // Convert path vector to logical path string (e.g., ["std", "io"] -> "std.io")
    std::string logicalPath;
    if (stmt->isFilePath) {
        // File paths are already in the first element
        logicalPath = stmt->path[0];
    } else {
        // Join path components with dots
        for (size_t i = 0; i < stmt->path.size(); ++i) {
            if (i > 0) logicalPath += ".";
            logicalPath += stmt->path[i];
        }
    }
    
    // Load the module using the UseStmt overload so that isFilePath is preserved.
    // The string-based overload re-derives isFilePath from the string content,
    // which fails for "wave.aria" (no slash, no leading dot) — it would be treated
    // as a logical dotted path instead of a file path.
    LoadedModule* module = moduleLoader->loadModule(stmt, currentModulePath);
    if (!module) {
        addError("Failed to load module '" + logicalPath + "'", stmt);
        // Add detailed errors from module loader
        for (const auto& err : moduleLoader->getErrors()) {
            addError("  " + err, stmt);
        }
        return;
    }
    
    // Type-check the loaded module if not already done
    // This populates the module's symbol table and exports
    if (module->state == ModuleState::FULLY_LOADED && module->moduleInfo) {
        // Check if we've already type-checked this module
        // (Simple check: if exports is empty, we haven't type-checked yet)
        if (module->moduleInfo->getExports().empty() && module->ast) {
            // Temporarily redirect currentModulePath to the module being processed
            // so that relative 'use' statements INSIDE that module (e.g. wavemech.aria
            // doing 'use "wave.aria".*;') resolve relative to the module's own directory.
            std::string savedModulePath = currentModulePath;
            currentModulePath = module->canonicalPath;

            // IMPORT PASS: Process any 'use' statements at the top of the module first
            // so that symbols from dependencies are in scope during type-checking.
            for (const auto& decl : module->ast->declarations) {
                if (decl->type == ASTNode::NodeType::USE) {
                    checkUseStmt(static_cast<UseStmt*>(decl.get()));
                }
            }

            // STRUCT PRE-PASS: register non-generic struct types BEFORE function
            // signatures so that struct names resolve correctly in param/return types.
            // Also register Type:Name struct layouts early for the same reason.
            for (const auto& decl : module->ast->declarations) {
                if (decl->type == ASTNode::NodeType::STRUCT_DECL) {
                    StructDeclStmt* sd = static_cast<StructDeclStmt*>(decl.get());
                    if (sd->genericParams.empty() && !typeSystem->getStructType(sd->structName)) {
                        checkStructDecl(sd);
                    }
                } else if (decl->type == ASTNode::NodeType::TYPE_DECL) {
                    TypeDeclStmt* td = static_cast<TypeDeclStmt*>(decl.get());
                    if (!typeSystem->getStructType(td->typeName)) {
                        // Pre-register the merged struct layout so method parameter/return
                        // types that reference TypeName (e.g. Number:self) can be resolved.
                        std::vector<ASTNodePtr> preFields;
                        if (td->internalStruct) {
                            auto is = std::static_pointer_cast<StructDeclStmt>(td->internalStruct);
                            for (const auto& f : is->fields) preFields.push_back(f);
                        }
                        if (td->interfaceStruct) {
                            auto is = std::static_pointer_cast<StructDeclStmt>(td->interfaceStruct);
                            for (const auto& f : is->fields) preFields.push_back(f);
                        }
                        auto preStruct = std::make_shared<StructDeclStmt>(
                            td->typeName, preFields, td->line, td->column);
                        checkStructDecl(preStruct.get());
                    }
                }
            }

            // PRE-PASS: Register all global variable and constant declarations first.
            // This must happen before type-checking functions, because module-level
            // functions may reference global variables defined in the same module.
            // Also register ALL function signatures (inc. private helpers) so that
            // public functions calling private helpers can resolve their types.
            // Note: STRUCT_DECL is handled in the main pass below (not idempotent).
            // Note: Constants use VAR_DECL with isConst=true — no separate node type.
            for (const auto& decl : module->ast->declarations) {
                if (decl->type == ASTNode::NodeType::VAR_DECL) {
                    checkVarDecl(static_cast<VarDeclStmt*>(decl.get()));
                } else if (decl->type == ASTNode::NodeType::FUNC_DECL) {
                    // Register ALL functions (public and private) so calls to private
                    // helpers inside public functions resolve correctly.
                    FuncDeclStmt* fd = static_cast<FuncDeclStmt*>(decl.get());
                    if (!fd->genericParams.empty()) continue; // generics handled later
                    std::vector<Type*> pts;
                    for (const auto& p : fd->parameters) {
                        if (p->type == ASTNode::NodeType::PARAMETER) {
                            ParameterNode* pn = static_cast<ParameterNode*>(p.get());
                            Type* pt = resolveTypeNode(pn->typeNode.get());
                            pts.push_back((pt && pt->getKind() != TypeKind::ERROR)
                                          ? pt : typeSystem->getPrimitiveType("void"));
                        }
                    }
                    // Return type must be wrapped in Result<T> for body functions,
                    // same as the standalone program pre-pass (see preRegisterFunctions).
                    Type* valueType = resolveTypeNode(fd->returnType.get());
                    if (!valueType) valueType = typeSystem->getPrimitiveType("void");
                    Type* retType = fd->body ? new ResultType(valueType) : valueType;
                    Type* ft = new FunctionType(pts, retType, fd->isAsync, false);
                    // Only define if not already in symbol table
                    if (!symbolTable->lookupSymbol(fd->funcName)) {
                        Symbol* sym = symbolTable->defineSymbol(fd->funcName,
                                          SymbolKind::FUNCTION, ft, fd->line, fd->column);
                        if (sym) sym->setFuncDecl(fd);
                    }
                } else if (decl->type == ASTNode::NodeType::EXTERN) {
                    // BUG-002 fix: Register extern block functions in symbol table
                    // so pub func wrappers in this module can reference them.
                    checkExternStmt(static_cast<ExternStmt*>(decl.get()));
                }
            }

            // MAIN PASS: Export PUBLIC top-level declarations
            for (const auto& decl : module->ast->declarations) {
                if (decl->type == ASTNode::NodeType::FUNC_DECL) {
                    FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(decl.get());
                    
                    // Only export public functions
                    if (!funcDecl->isPublic) {
                        continue;
                    }
                    
                    // Type-check the function declaration (handles both generic and non-generic)
                    // This registers generic functions for later monomorphization
                    checkFuncDecl(funcDecl);
                    
                    // Build the function type from the declaration
                    // First, resolve parameter types
                    std::vector<Type*> paramTypes;
                    
                    if (!funcDecl->genericParams.empty()) {
                        // Generic function - use placeholder types
                        for (const auto& param : funcDecl->parameters) {
                            paramTypes.push_back(typeSystem->getUnknownType());
                        }
                    } else {
                        // Non-generic function - resolve actual types
                        for (const auto& param : funcDecl->parameters) {
                            if (param->type == ASTNode::NodeType::PARAMETER) {
                                ParameterNode* paramNode = static_cast<ParameterNode*>(param.get());
                                Type* paramType = resolveTypeNode(paramNode->typeNode.get());
                                if (paramType && paramType->getKind() != TypeKind::ERROR) {
                                    paramTypes.push_back(paramType);
                                } else {
                                    // If we can't resolve param type, use void as fallback
                                    paramTypes.push_back(typeSystem->getPrimitiveType("void"));
                                }
                            }
                        }
                    }
                    
                    // Resolve return type
                    Type* returnType = nullptr;
                    if (!funcDecl->genericParams.empty()) {
                        // Generic function - use placeholder return type
                        returnType = typeSystem->getUnknownType();
                    } else {
                        returnType = resolveTypeNode(funcDecl->returnType.get());
                        if (!returnType || returnType->getKind() == TypeKind::ERROR) {
                            returnType = typeSystem->getPrimitiveType("void");
                        }
                        // BUG-001 fix: Non-extern functions use pass() which wraps
                        // the return value in Result<T>. The export must mirror this
                        // so importers see Result<T>, not the raw declared type.
                        // (Matches pre-registration pass at line ~11139)
                        if (funcDecl->body) {
                            returnType = new ResultType(returnType);
                        }
                    }
                    
                    // Create the function type
                    Type* funcType = new FunctionType(paramTypes, returnType, funcDecl->isAsync, false);
                    
                    // Create a new symbol for export
                    Symbol* funcSym = new Symbol(funcDecl->funcName, SymbolKind::FUNCTION, funcType, nullptr, funcDecl->line, funcDecl->column);
                    funcSym->funcDecl = funcDecl;  // Store AST for generic resolution
                    
                    // Export the function as PUBLIC
                    module->moduleInfo->exportSymbol(funcDecl->funcName, funcSym, Visibility::PUBLIC);
                }
                else if (decl->type == ASTNode::NodeType::STRUCT_DECL) {
                    StructDeclStmt* structDecl = static_cast<StructDeclStmt*>(decl.get());
                    
                    // Type-check the struct declaration to register generic structs
                    // and create proper types in the type system
                    checkStructDecl(structDecl);
                    
                    // Get the actual struct type from the type system
                    Type* structType = nullptr;
                    if (!structDecl->genericParams.empty()) {
                        // Generic struct - it's registered but not instantiated yet
                        structType = typeSystem->getPrimitiveType("void");  // Placeholder for generic
                    } else {
                        // Non-generic struct - should be in type system now
                        structType = typeSystem->getStructType(structDecl->structName);
                        if (!structType) {
                            structType = typeSystem->getPrimitiveType("void");  // Fallback
                        }
                    }
                    
                    Symbol* structSym = new Symbol(structDecl->structName, SymbolKind::TYPE, structType, nullptr, structDecl->line, structDecl->column);
                    
                    // Export the struct as PUBLIC (for now, export all structs)
                    // TODO: Add pub modifier to struct declarations
                    module->moduleInfo->exportSymbol(structDecl->structName, structSym, Visibility::PUBLIC);
                }
                else if (decl->type == ASTNode::NodeType::TYPE_DECL) {
                    TypeDeclStmt* typeDecl = static_cast<TypeDeclStmt*>(decl.get());

                    // The struct layout was pre-registered in the struct pre-pass above.
                    // Here we rename and type-check each method function, then export
                    // both the struct type and all mangled method names.
                    //
                    // We do NOT call checkTypeDecl() because it would re-register the
                    // struct and error with "Type already defined".

                    // Helper: rename func → TypeName_func, type-check, export
                    auto processAndExport = [&](FuncDeclStmt* func) {
                        std::string mangledName = typeDecl->typeName + "_" + func->funcName;
                        func->funcName = mangledName;
                        checkFuncDecl(func);
                        Symbol* sym = symbolTable->lookupSymbol(mangledName);
                        if (sym) {
                            module->moduleInfo->exportSymbol(mangledName, sym, Visibility::PUBLIC);
                        }
                    };

                    if (typeDecl->createFunc) {
                        auto cf = std::static_pointer_cast<FuncDeclStmt>(typeDecl->createFunc);
                        processAndExport(cf.get());
                    }
                    if (typeDecl->destroyFunc) {
                        auto df = std::static_pointer_cast<FuncDeclStmt>(typeDecl->destroyFunc);
                        processAndExport(df.get());
                    }
                    for (const auto& method : typeDecl->methods) {
                        auto mf = std::static_pointer_cast<FuncDeclStmt>(method);
                        processAndExport(mf.get());
                    }

                    // Export the struct type itself (e.g. "Number")
                    Type* typeStructType = typeSystem->getStructType(typeDecl->typeName);
                    if (typeStructType) {
                        Symbol* typeSym = new Symbol(typeDecl->typeName, SymbolKind::TYPE,
                                                     typeStructType, nullptr,
                                                     typeDecl->line, typeDecl->column);
                        module->moduleInfo->exportSymbol(typeDecl->typeName, typeSym, Visibility::PUBLIC);
                    }
                }
                else if (decl->type == ASTNode::NodeType::EXTERN) {
                    // BUG-002 fix: Export extern block functions so importers can
                    // reference them (both directly and through pub func wrappers).
                    // Extern functions use raw return types (no Result<T> wrapping).
                    ExternStmt* externStmt = static_cast<ExternStmt*>(decl.get());
                    for (const auto& extDecl : externStmt->declarations) {
                        if (extDecl->type == ASTNode::NodeType::FUNC_DECL) {
                            FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(extDecl.get());
                            // Look up the symbol registered by checkExternStmt in the pre-pass
                            Symbol* sym = symbolTable->lookupSymbol(funcDecl->funcName);
                            if (sym) {
                                module->moduleInfo->exportSymbol(funcDecl->funcName, sym, Visibility::PUBLIC);
                            }
                        }
                    }
                }
            }
            
            // Mark module as type-checked to avoid re-processing
            module->state = ModuleState::CHECKED;

            // Restore the caller's module path
            currentModulePath = savedModulePath;
        }
    }
    
    // Import symbols based on import type
    if (stmt->isWildcard) {
        // Wildcard import: import all public symbols directly into current scope
        importWildcardSymbols(module, stmt);
    } else if (!stmt->items.empty()) {
        // Selective import: import only specified symbols
        importSelectiveSymbols(module, stmt);
    } else {
        // Namespace import: import module as a namespace
        importModuleNamespace(module, stmt, logicalPath);
    }
}

void TypeChecker::checkModStmt(ModStmt* stmt) {
    if (!stmt) {
        return;
    }
    
    // Validate module name
    if (stmt->name.empty()) {
        addError("Module name cannot be empty", stmt);
        return;
    }
    
    // Check for invalid characters in module name
    // Module names should be valid identifiers
    for (char c : stmt->name) {
        if (!std::isalnum(c) && c != '_') {
            addError("Invalid character '" + std::string(1, c) + "' in module name '" + 
                    stmt->name + "'. Module names must be valid identifiers.", stmt);
            return;
        }
    }
    
    // If it's an inline module, type check all declarations in its body
    if (stmt->isInline) {
        // Enter module scope (for nested symbol resolution)
        // For now, just check each declaration
        for (const auto& decl : stmt->body) {
            if (decl) {
                checkStatement(decl.get());
            }
        }
    }
    
    // External module declarations (mod name;) don't need further checking
    // The module resolver will validate they exist
}

// ============================================================================
// Transitive Monomorphization — analyze specialized function bodies
// ============================================================================

void TypeChecker::analyzeSpecializedBody(FuncDeclStmt* specDecl, const TypeSubstitution& outerSub) {
    if (!specDecl || !specDecl->body || !genericResolver || !monomorphizer) return;

    // Recursive AST walker to find CallExpr nodes referencing generic functions
    std::function<void(ASTNode*)> walkNode = [&](ASTNode* node) {
        if (!node) return;

        if (node->type == ASTNode::NodeType::CALL) {
            CallExpr* call = static_cast<CallExpr*>(node);
            if (call->callee && call->callee->type == ASTNode::NodeType::IDENTIFIER) {
                IdentifierExpr* id = static_cast<IdentifierExpr*>(call->callee.get());

                // Check if this callee is a generic function
                Symbol* sym = symbolTable->lookupSymbol(id->name);
                FuncDeclStmt* calledFunc = nullptr;
                if (sym && sym->kind == SymbolKind::FUNCTION) {
                    calledFunc = sym->getFuncDecl();
                }
                if (calledFunc && !calledFunc->genericParams.empty() &&
                    call->specializedMangledName.empty()) {
                    // Build substitution for the nested generic call using
                    // the outer specialization's type mapping.
                    TypeSubstitution sub;
                    for (const auto& gp : calledFunc->genericParams) {
                        auto it = outerSub.find(gp.name);
                        if (it != outerSub.end()) {
                            sub[gp.name] = it->second;
                        }
                    }

                    if (!sub.empty() && sub.size() == calledFunc->genericParams.size()) {
                        Specialization* innerSpec =
                            monomorphizer->requestSpecialization(calledFunc, sub);
                        if (innerSpec) {
                            call->specializedMangledName = innerSpec->mangledName;
                            // Recurse into the newly specialized body
                            analyzeSpecializedBody(innerSpec->funcDecl, sub);
                        }
                    }
                }
            }
            // Walk call arguments
            for (const auto& arg : call->arguments) {
                walkNode(arg.get());
            }
            return;
        }

        // Walk children for common node types
        switch (node->type) {
            case ASTNode::NodeType::BLOCK: {
                BlockStmt* block = static_cast<BlockStmt*>(node);
                for (const auto& s : block->statements) walkNode(s.get());
                break;
            }
            case ASTNode::NodeType::VAR_DECL: {
                VarDeclStmt* vd = static_cast<VarDeclStmt*>(node);
                if (vd->initializer) walkNode(vd->initializer.get());
                break;
            }
            case ASTNode::NodeType::EXPRESSION_STMT: {
                ExpressionStmt* es = static_cast<ExpressionStmt*>(node);
                if (es->expression) walkNode(es->expression.get());
                break;
            }
            case ASTNode::NodeType::IF: {
                IfStmt* ifs = static_cast<IfStmt*>(node);
                if (ifs->condition) walkNode(ifs->condition.get());
                if (ifs->thenBranch) walkNode(ifs->thenBranch.get());
                if (ifs->elseBranch) walkNode(ifs->elseBranch.get());
                break;
            }
            case ASTNode::NodeType::WHILE: {
                WhileStmt* ws = static_cast<WhileStmt*>(node);
                if (ws->condition) walkNode(ws->condition.get());
                if (ws->body) walkNode(ws->body.get());
                break;
            }
            case ASTNode::NodeType::FOR: {
                ForStmt* fs = static_cast<ForStmt*>(node);
                if (fs->initializer) walkNode(fs->initializer.get());
                if (fs->condition) walkNode(fs->condition.get());
                if (fs->update) walkNode(fs->update.get());
                if (fs->body) walkNode(fs->body.get());
                break;
            }
            case ASTNode::NodeType::RETURN: {
                ReturnStmt* rs = static_cast<ReturnStmt*>(node);
                if (rs->value) walkNode(rs->value.get());
                break;
            }
            case ASTNode::NodeType::PASS: {
                PassStmt* ps = static_cast<PassStmt*>(node);
                if (ps->value) walkNode(ps->value.get());
                break;
            }
            case ASTNode::NodeType::FAIL: {
                FailStmt* fs = static_cast<FailStmt*>(node);
                if (fs->errorCode) walkNode(fs->errorCode.get());
                break;
            }
            case ASTNode::NodeType::BINARY_OP: {
                BinaryExpr* be = static_cast<BinaryExpr*>(node);
                if (be->left) walkNode(be->left.get());
                if (be->right) walkNode(be->right.get());
                break;
            }
            case ASTNode::NodeType::UNARY_OP: {
                UnaryExpr* ue = static_cast<UnaryExpr*>(node);
                if (ue->operand) walkNode(ue->operand.get());
                break;
            }
            case ASTNode::NodeType::ASSIGNMENT: {
                BinaryExpr* ae = static_cast<BinaryExpr*>(node);
                if (ae->left) walkNode(ae->left.get());
                if (ae->right) walkNode(ae->right.get());
                break;
            }
            default:
                break;
        }
    };

    walkNode(specDecl->body.get());
}

void TypeChecker::checkExternStmt(ExternStmt* stmt) {
    if (!stmt) {
        return;
    }
    
    // Register all extern function and variable declarations in symbol table
    // This allows them to be referenced by other code in the same scope
    
    for (const auto& decl : stmt->declarations) {
        if (decl->type == ASTNode::NodeType::FUNC_DECL) {
            // Register extern function
            FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(decl.get());
            
            // Resolve return type
            Type* returnType = resolveTypeNode(funcDecl->returnType.get());
            
            // Resolve parameter types
            std::vector<Type*> paramTypes;
            for (const auto& param : funcDecl->parameters) {
                if (param->type == ASTNode::NodeType::PARAMETER) {
                    ParameterNode* paramNode = static_cast<ParameterNode*>(param.get());
                    Type* paramType = resolveTypeNode(paramNode->typeNode.get());
                    paramTypes.push_back(paramType);
                }
            }
            
            // Create function type (extern functions are never async or generic)
            Type* funcType = new FunctionType(paramTypes, returnType, false, false);
            
            // Register in symbol table
            symbolTable->defineSymbol(funcDecl->funcName, SymbolKind::FUNCTION, funcType);
        }
        else if (decl->type == ASTNode::NodeType::VAR_DECL) {
            // Register extern variable
            VarDeclStmt* varDecl = static_cast<VarDeclStmt*>(decl.get());
            
            // Resolve variable type
            Type* varType = nullptr;
            if (varDecl->typeNode) {
                varType = resolveTypeNode(varDecl->typeNode.get());
            } else if (!varDecl->typeName.empty()) {
                // Legacy simple type name
                varType = typeSystem->getPrimitiveType(varDecl->typeName);
                if (!varType || varType->getKind() == TypeKind::ERROR) {
                    varType = typeSystem->getStructType(varDecl->typeName);
                }
            }
            
            if (varType && varType->getKind() != TypeKind::ERROR) {
                symbolTable->defineSymbol(varDecl->varName, SymbolKind::VARIABLE, varType);
            }
        }
        else if (decl->type == ASTNode::NodeType::OPAQUE_STRUCT) {
            // Register opaque struct type
            OpaqueStructDecl* opaqueDecl = static_cast<OpaqueStructDecl*>(decl.get());
            
            // Create opaque struct type (empty struct as placeholder)
            Type* opaqueType = typeSystem->createStructType(opaqueDecl->structName, {});
            
            // Mark as opaque in symbol table (type is already registered in typeSystem)
            // The type will be treated as a pointer type in IR generation
        }
    }
}

// ============================================================================
// Module Symbol Importing (Phase 3 from research_module_loading_system)
// ============================================================================

void TypeChecker::importWildcardSymbols(LoadedModule* module, UseStmt* stmt) {
    if (!module || !module->moduleInfo) {
        return;
    }
    
    // Get the module's export table via public getter
    const auto& exports = module->moduleInfo->getExports();
    
    // Import all PUBLIC symbols into current scope
    for (const auto& [name, exportEntry] : exports) {
        if (exportEntry.visibility != Visibility::PUBLIC) {
            continue;  // Skip non-public symbols
        }
        
        // If a symbol with this name already exists (e.g. registered by the pre-pass
        // with a stale type before structs were resolved), UPDATE it with the correct
        // type from the export table. The MAIN PASS always produces authoritative types.
        if (symbolTable->isDefined(name)) {
            Symbol* existing = symbolTable->lookupSymbol(name);
            if (existing) {
                existing->type = exportEntry.symbol->type;
                if (exportEntry.symbol->funcDecl) {
                    existing->funcDecl = exportEntry.symbol->funcDecl;
                }
            }
            continue;
        }
        
        // Add symbol to current scope
        symbolTable->defineSymbol(name, exportEntry.symbol->kind, exportEntry.symbol->type);
    }
}

void TypeChecker::importSelectiveSymbols(LoadedModule* module, UseStmt* stmt) {
    if (!module || !module->moduleInfo) {
        return;
    }
    
    // Get the module's export table via public getter
    const auto& exports = module->moduleInfo->getExports();
    
    // Import each specified symbol
    // Note: items is a vector<string>, so we just import each name
    for (const std::string& symbolName : stmt->items) {
        // Check if symbol exists in module
        auto it = exports.find(symbolName);
        if (it == exports.end()) {
            addError("Symbol '" + symbolName + "' not found in module", stmt);
            continue;
        }
        
        const auto& exportEntry = it->second;
        
        // Check visibility
        if (exportEntry.visibility != Visibility::PUBLIC) {
            addError("Symbol '" + symbolName + "' is not public and cannot be imported", stmt);
            continue;
        }
        
        // Check for name collision
        if (symbolTable->isDefined(symbolName)) {
            addError("Symbol '" + symbolName + "' already defined in current scope", stmt);
            continue;
        }
        
        // Add symbol to current scope
        symbolTable->defineSymbol(symbolName, exportEntry.symbol->kind, exportEntry.symbol->type);
    }
}

void TypeChecker::importModuleNamespace(LoadedModule* module, UseStmt* stmt, const std::string& modulePath) {
    if (!module || !module->moduleInfo) {
        return;
    }
    
    // Determine namespace name (last component of module path or alias)
    std::string namespaceName;
    if (!stmt->alias.empty()) {
        namespaceName = stmt->alias;
    } else if (stmt->isFilePath) {
        // For file paths, extract filename without extension
        // e.g., "./helper.aria" -> "helper", "../utils/math.aria" -> "math"
        std::string filePath = module->moduleInfo->getPath();
        size_t lastSlash = filePath.find_last_of("/\\");
        std::string filename = (lastSlash != std::string::npos) ? 
                               filePath.substr(lastSlash + 1) : filePath;
        
        // Remove .aria extension
        size_t lastDot = filename.find_last_of('.');
        namespaceName = (lastDot != std::string::npos) ? 
                        filename.substr(0, lastDot) : filename;
    } else {
        // For logical paths, extract last component (e.g., "std.io" -> "io")
        size_t lastDot = modulePath.find_last_of('.');
        if (lastDot != std::string::npos) {
            namespaceName = modulePath.substr(lastDot + 1);
        } else {
            namespaceName = modulePath;
        }
    }
    
    // Check for namespace collision
    if (symbolTable->isDefined(namespaceName)) {
        addError("Namespace '" + namespaceName + "' already defined in current scope", stmt);
        return;
    }
    
    // Create a MODULE symbol with void type as placeholder
    Type* namespaceType = typeSystem->getPrimitiveType("void");
    Symbol* moduleSym = symbolTable->defineSymbol(namespaceName, SymbolKind::MODULE, namespaceType);
    
    // Store the module reference in the symbol for later resolution
    if (moduleSym) {
        moduleSym->setModuleRef(module->moduleInfo.get());
        loadedModules[namespaceName] = module;  // Track loaded module
    }
}

// ============================================================================
// Advanced Loop Type Checking
// ============================================================================

void TypeChecker::checkTillStmt(TillStmt* stmt) {
    // Check limit expression - must be integer type
    Type* limitType = inferType(stmt->limit.get());
    if (limitType->getKind() != TypeKind::ERROR) {
        PrimitiveType* limitPrim = dynamic_cast<PrimitiveType*>(limitType);
        if (!limitPrim || (limitPrim->getName().find("int") == std::string::npos && 
                           limitPrim->getName().find("uint") == std::string::npos)) {
            addError("till limit must be integer type, got '" + limitType->toString() + "'", 
                    stmt->limit.get());
        }
    }
    
    // Check step expression - must be integer type
    Type* stepType = inferType(stmt->step.get());
    if (stepType->getKind() != TypeKind::ERROR) {
        PrimitiveType* stepPrim = dynamic_cast<PrimitiveType*>(stepType);
        if (!stepPrim || (stepPrim->getName().find("int") == std::string::npos && 
                          stepPrim->getName().find("uint") == std::string::npos)) {
            addError("till step must be integer type, got '" + stepType->toString() + "'", 
                    stmt->step.get());
        }
    }
    
    // Check body
    checkStatement(stmt->body.get());
}

void TypeChecker::checkLoopStmt(LoopStmt* stmt) {
    // Check start expression - must be integer type
    Type* startType = inferType(stmt->start.get());
    if (startType->getKind() != TypeKind::ERROR) {
        PrimitiveType* startPrim = dynamic_cast<PrimitiveType*>(startType);
        if (!startPrim || (startPrim->getName().find("int") == std::string::npos && 
                           startPrim->getName().find("uint") == std::string::npos)) {
            addError("loop start must be integer type, got '" + startType->toString() + "'", 
                    stmt->start.get());
        }
    }
    
    // Check limit expression - must be integer type
    Type* limitType = inferType(stmt->limit.get());
    if (limitType->getKind() != TypeKind::ERROR) {
        PrimitiveType* limitPrim = dynamic_cast<PrimitiveType*>(limitType);
        if (!limitPrim || (limitPrim->getName().find("int") == std::string::npos && 
                           limitPrim->getName().find("uint") == std::string::npos)) {
            addError("loop limit must be integer type, got '" + limitType->toString() + "'", 
                    stmt->limit.get());
        }
    }
    
    // Check step expression - must be integer type
    Type* stepType = inferType(stmt->step.get());
    if (stepType->getKind() != TypeKind::ERROR) {
        PrimitiveType* stepPrim = dynamic_cast<PrimitiveType*>(stepType);
        if (!stepPrim || (stepPrim->getName().find("int") == std::string::npos && 
                          stepPrim->getName().find("uint") == std::string::npos)) {
            addError("loop step must be integer type, got '" + stepType->toString() + "'", 
                    stmt->step.get());
        }
    }
    
    // Check body
    checkStatement(stmt->body.get());
}

void TypeChecker::checkWhenStmt(WhenStmt* stmt) {
    // Check condition - must be boolean type
    Type* condType = inferType(stmt->condition.get());
    if (condType->getKind() != TypeKind::ERROR) {
        PrimitiveType* condPrim = dynamic_cast<PrimitiveType*>(condType);
        if (!condPrim || condPrim->getName() != "bool") {
            addError("when condition must be 'bool' type, got '" + condType->toString() + "'", 
                    stmt->condition.get());
        }
    }
    
    // Check body
    checkStatement(stmt->body.get());
    
    // Check then block if present
    if (stmt->then_block) {
        checkStatement(stmt->then_block.get());
    }
    
    // Check end block if present
    if (stmt->end_block) {
        checkStatement(stmt->end_block.get());
    }
}

void TypeChecker::checkPickStmt(PickStmt* stmt) {
    // Infer the selector type
    Type* selectorType = inferType(stmt->selector.get());
    
    // Only perform exhaustiveness checking if selector type is valid
    if (selectorType->getKind() != TypeKind::ERROR) {
        // Check exhaustiveness
        ExhaustivenessAnalyzer::Analysis result = ExhaustivenessAnalyzer::analyze(stmt, selectorType);
        
        // Report error if not exhaustive
        if (!result.isExhaustive) {
            addError(result.errorMessage, stmt);
        }
    }
    
    // Phase 3.5: Result enforcement for pick statements
    // Save current state before pick
    ResultCheckState beforePick = currentResultState;
    std::vector<ResultCheckState> caseStates;
    
    // Type-check all case bodies with Result knowledge
    for (const auto& caseNode : stmt->cases) {
        PickCase* pickCase = static_cast<PickCase*>(caseNode.get());
        
        // Start with state from before pick
        ResultCheckState caseState = beforePick;
        
        // Check if selector is .is_error field access on a Result variable
        // If so, and pattern is a boolean literal, propagate knowledge
        if (stmt->selector->type == ASTNode::NodeType::MEMBER_ACCESS) {
            MemberAccessExpr* memberAccess = static_cast<MemberAccessExpr*>(stmt->selector.get());
            if (memberAccess->member == "is_error") {
                // Get the Result variable name
                std::string resultVar;
                if (memberAccess->object->type == ASTNode::NodeType::IDENTIFIER) {
                    IdentifierExpr* objIdent = static_cast<IdentifierExpr*>(memberAccess->object.get());
                    resultVar = objIdent->name;
                }
                
                // Check if pattern is a boolean literal
                if (!resultVar.empty() && pickCase->pattern->type == ASTNode::NodeType::LITERAL) {
                    LiteralExpr* patternLit = static_cast<LiteralExpr*>(pickCase->pattern.get());
                    if (std::holds_alternative<bool>(patternLit->value)) {
                        bool isError = std::get<bool>(patternLit->value);
                        
                        if (isError) {
                            // Case matches when is_error == true
                            caseState.markKnownError(resultVar);
                        } else {
                            // Case matches when is_error == false
                            caseState.markKnownSuccess(resultVar);
                        }
                    }
                }
            }
        }
        
        // Check case body with derived knowledge
        currentResultState = caseState;
        if (pickCase->body) {
            checkStatement(pickCase->body.get());
        }
        
        // Save state after this case
        caseStates.push_back(currentResultState);
    }
    
    // After pick: merge all case states conservatively
    if (!caseStates.empty()) {
        currentResultState = caseStates[0];
        for (size_t i = 1; i < caseStates.size(); i++) {
            currentResultState = ResultCheckState::merge(currentResultState, caseStates[i]);
        }
    } else {
        // No cases - keep before state
        currentResultState = beforePick;
    }
}

// ============================================================================
// Result<T> "No Checky No Val" Enforcement
// ============================================================================

/**
 * Mark a Result variable as checked
 * 
 * Called when code accesses .is_error on a Result variable.
 * This allows subsequent .value or .error access on the same variable.
 * 
 * Phase 2: Uses ResultCheckState for control flow tracking
 */
void TypeChecker::markResultChecked(const std::string& varName) {
    if (!varName.empty()) {
        currentResultState.markChecked(varName);
    }
}

/**
 * Check if a Result variable has been checked
 * 
 * Returns true if .is_error was accessed on this variable.
 */
bool TypeChecker::isResultChecked(const std::string& varName) const {
    return currentResultState.isChecked(varName);
}

/**
 * Get variable name from an expression node
 * 
 * Used for tracking Result variables in "no checky no val" enforcement.
 * 
 * Handles:
 * - IDENTIFIER: Simple variable (r.is_error)
 * - MEMBER_ACCESS: Struct field (obj.result_field.is_error)
 * 
 * Returns empty string for complex expressions (function calls, etc.)
 * These bypass tracking for now - Phase 2 will handle more cases.
 */
std::string TypeChecker::getVariableName(ASTNode* expr) const {
    if (!expr) return "";
    
    if (expr->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr);
        return ident->name;
    }
    
    if (expr->type == ASTNode::NodeType::MEMBER_ACCESS) {
        MemberAccessExpr* member = static_cast<MemberAccessExpr*>(expr);
        std::string baseName = getVariableName(member->object.get());
        if (!baseName.empty()) {
            return baseName + "." + member->member;
        }
    }
    
    // Complex expression - can't track for now
    return "";
}

// ============================================================================
// Phase 2: Control Flow Analysis for Result Checking
// ============================================================================

/**
 * Analyze an if condition to extract Result check knowledge
 * 
 * Detects patterns like:
 * - r.is_error == true  → then knows ERROR, else knows SUCCESS
 * - r.is_error == false → then knows SUCCESS, else knows ERROR
 * - r.is_error != true  → then knows SUCCESS, else knows ERROR
 * - !r.is_error         → then knows SUCCESS, else knows ERROR
 * - !(r.is_error)       → then knows SUCCESS, else knows ERROR
 * 
 * Updates thenState and elseState with what each branch knows
 */
void TypeChecker::analyzeConditionForResultChecks(
    ASTNode* condition,
    ResultCheckState& thenState,
    ResultCheckState& elseState
) {
    using namespace frontend;
    
    // Handle negation: !condition
    if (condition->type == ASTNode::NodeType::UNARY_OP) {
        UnaryExpr* unaryExpr = static_cast<UnaryExpr*>(condition);
        
        // Only handle logical NOT
        if (unaryExpr->op.type == TokenType::TOKEN_BANG) {
            // Recursively analyze the negated condition
            // But flip then/else since negation inverts the condition
            analyzeConditionForResultChecks(unaryExpr->operand.get(), elseState, thenState);
            return;
        }
    }
    
    // Handle direct boolean member access: if (r.is_error) { ... }
    if (condition->type == ASTNode::NodeType::MEMBER_ACCESS) {
        MemberAccessExpr* memberAccess = static_cast<MemberAccessExpr*>(condition);
        
        // Check if member is .is_error
        if (memberAccess->member == "is_error") {
            std::string varName = getVariableName(memberAccess->object.get());
            if (!varName.empty()) {
                // if (r.is_error) → then knows ERROR, else knows SUCCESS
                thenState.markKnownError(varName);
                elseState.markKnownSuccess(varName);
                return;
            }
        }
    }
    
    // Only analyze binary comparison expressions
    if (condition->type != ASTNode::NodeType::BINARY_OP) {
        return;
    }
    
    BinaryExpr* binExpr = static_cast<BinaryExpr*>(condition);
    
    // Only analyze == and != comparisons
    if (binExpr->op.type != TokenType::TOKEN_EQUAL_EQUAL &&
        binExpr->op.type != TokenType::TOKEN_BANG_EQUAL) {
        return;
    }
    
    // Check if one side is .is_error member access, other is boolean literal
    MemberAccessExpr* memberAccess = nullptr;
    LiteralExpr* literal = nullptr;
    
    // Check left.is_error == right
    if (binExpr->left->type == ASTNode::NodeType::MEMBER_ACCESS &&
        binExpr->right->type == ASTNode::NodeType::LITERAL) {
        memberAccess = static_cast<MemberAccessExpr*>(binExpr->left.get());
        literal = static_cast<LiteralExpr*>(binExpr->right.get());
    }
    // Check left == right.is_error
    else if (binExpr->right->type == ASTNode::NodeType::MEMBER_ACCESS &&
             binExpr->left->type == ASTNode::NodeType::LITERAL) {
        memberAccess = static_cast<MemberAccessExpr*>(binExpr->right.get());
        literal = static_cast<LiteralExpr*>(binExpr->left.get());
    }
    
    // Not a pattern we recognize
    if (!memberAccess || !literal) {
        return;
    }
    
    // Check if member is .is_error
    if (memberAccess->member != "is_error") {
        return;
    }
    
    // Check if literal is boolean
    if (!std::holds_alternative<bool>(literal->value)) {
        return;
    }
    
    // Get the Result variable name
    std::string varName = getVariableName(memberAccess->object.get());
    if (varName.empty()) {
        return;
    }
    
    // Get the boolean value being compared
    bool errorValue = std::get<bool>(literal->value);
    
    // Determine what each branch knows based on comparison
    bool comparing_equal = (binExpr->op.type == TokenType::TOKEN_EQUAL_EQUAL);
    
    if (comparing_equal) {
        // r.is_error == true or r.is_error == false
        if (errorValue) {
            // r.is_error == true
            thenState.markKnownError(varName);
            elseState.markKnownSuccess(varName);
        } else {
            // r.is_error == false
            thenState.markKnownSuccess(varName);
            elseState.markKnownError(varName);
        }
    } else {
        // r.is_error != true or r.is_error != false
        if (errorValue) {
            // r.is_error != true (means is_error == false)
            thenState.markKnownSuccess(varName);
            elseState.markKnownError(varName);
        } else {
            // r.is_error != false (means is_error == true)
            thenState.markKnownError(varName);
            elseState.markKnownSuccess(varName);
        }
    }
}

/**
 * Check if a branch always returns/exits
 * 
 * Used for early return analysis:
 *   if (r.is_error) { return; }
 *   // After this, we know r.is_error == false
 * 
 * Also detects break/continue for loop analysis:
 *   while (true) {
 *     if (r.is_error) break;
 *     // After: knows !r.is_error
 *   }
 * 
 * Phase 2: Simple check for return at end of block
 * Phase 3: Also checks for break/continue
 */
bool TypeChecker::branchAlwaysReturns(ASTNode* branch) {
    if (!branch) {
        return false;
    }
    
    // Check block statements
    if (branch->type == ASTNode::NodeType::BLOCK) {
        BlockStmt* block = static_cast<BlockStmt*>(branch);
        if (block->statements.empty()) {
            return false;
        }
        
        // Check if last statement is return/pass/fail/break/continue
        ASTNode* last = block->statements.back().get();
        return last->type == ASTNode::NodeType::RETURN ||
               last->type == ASTNode::NodeType::PASS ||
               last->type == ASTNode::NodeType::FAIL ||
               last->type == ASTNode::NodeType::BREAK ||
               last->type == ASTNode::NodeType::CONTINUE;
    }
    
    // Single statement (no block)
    return branch->type == ASTNode::NodeType::RETURN ||
           branch->type == ASTNode::NodeType::PASS ||
           branch->type == ASTNode::NodeType::FAIL ||
           branch->type == ASTNode::NodeType::BREAK ||
           branch->type == ASTNode::NodeType::CONTINUE;
}

// ============================================================================
// Program Validation
// ============================================================================

bool TypeChecker::validateFailsafeExists() {
    // Check if failsafe() function is defined in the symbol table
    Symbol* failsafeSymbol = symbolTable->resolveSymbol("failsafe");
    
    if (!failsafeSymbol) {
        addError("Every Aria program must define a 'func:failsafe = int32(tbb32:err)' function. "
                 "failsafe is one of two program endpoints (with main). "
                 "It handles error exits via exit(code) where code > 0.", nullptr);
        return false;
    }
    
    // Validate that it's actually a function
    if (failsafeSymbol->type->getKind() != TypeKind::FUNCTION) {
        addError("'failsafe' must be a function, but is defined as: " + 
                 failsafeSymbol->type->toString(), nullptr);
        return false;
    }
    
    // Signature details validated in checkFuncDecl() when the function is parsed
    
    return true;
}

bool TypeChecker::validateMainExists() {
    // Check if main() function is defined in the symbol table
    Symbol* mainSymbol = symbolTable->resolveSymbol("main");
    
    if (!mainSymbol) {
        addError("Every Aria program must define a 'func:main = int32()' function. "
                 "main is one of two program endpoints (with failsafe). "
                 "It is the entry point and must call exit(code) where code 0 = success.", nullptr);
        return false;
    }
    
    // Validate that it's actually a function
    if (mainSymbol->type->getKind() != TypeKind::FUNCTION) {
        addError("'main' must be a function, but is defined as: " + 
                 mainSymbol->type->toString(), nullptr);
        return false;
    }
    
    // Signature details validated in checkFuncDecl() when the function is parsed
    
    return true;
}

} // namespace sema
} // namespace aria
