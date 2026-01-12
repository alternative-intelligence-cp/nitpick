#include "frontend/sema/type_checker.h"
#include "frontend/sema/generic_resolver.h"
#include "frontend/sema/exhaustiveness.h"
#include "frontend/sema/definite_assignment.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/type.h"  // For ArrayType, SimpleType, etc.
#include "semantic/literal_converter.h"
#include <sstream>
#include <variant>
#include <iostream>
#include <unordered_set>
#include <set>

namespace aria {
namespace sema {

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
        "bool", "string", "void", "NIL", "obj", "dyn",
        // Collections
        "array", "map", "Result", "option"
    };
    return typeKeywords.count(name) > 0;
}

// ============================================================================
// AST Traversal: Main Entry Point
// ============================================================================

void TypeChecker::check(ASTNode* module) {
    if (!module) {
        addError("Null module node", 0, 0);
        return;
    }
    
    // Module should be a BLOCK or PROGRAM node containing statements
    if (module->type == ASTNode::NodeType::BLOCK) {
        BlockStmt* blockStmt = static_cast<BlockStmt*>(module);
        for (const auto& stmt : blockStmt->statements) {
            if (stmt) {
                checkStatement(stmt.get());
            }
        }
    } else if (module->type == ASTNode::NodeType::PROGRAM) {
        // If PROGRAM node exists, it likely contains a body (assume BlockStmt)
        // For now, treat as block with statements vector
        BlockStmt* blockStmt = static_cast<BlockStmt*>(module);
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
        // Convert type suffix to type system name
        std::string type_name = typeSuffixToSystemName(expr->explicit_type);
        
        // Get type from type system
        Type* explicit_type = typeSystem->getPrimitiveType(type_name);
        
        // Validate type exists
        if (!explicit_type || explicit_type == typeSystem->getErrorType()) {
            addError("Unknown type '" + expr->explicit_type + "' in literal", expr);
            return typeSystem->getErrorType();
        }
        
        return explicit_type;
    }
    
    // ========================================================================
    // UNTYPED LITERALS: Return "unknown" type for context resolution
    // ========================================================================
    // NOTE: Type checker will validate range when target type is known
    // This enables "fits-or-fails" behavior:
    //   int8:a = 42;     ✅ OK: 42 fits in int8
    //   int8:b = 200;    ❌ ERROR: doesn't fit
    //   print(42);       ❌ ERROR: ambiguous context
    
    // Check if this is a high-precision literal (has raw string)
    if (expr->hasRawValue()) {
        // High-precision literal - type will be determined by context
        // Return unknown type so type checker validates range when assigned
        return typeSystem->getUnknownType();
    }
    
    // Use std::visit to handle variant (standard literals without raw strings)
    return std::visit([this](auto&& arg) -> Type* {
        using T = std::decay_t<decltype(arg)>;
        
        if constexpr (std::is_same_v<T, int64_t>) {
            // Untyped integer literal: return unknown for context inference
            return typeSystem->getUnknownType();
        }
        else if constexpr (std::is_same_v<T, double>) {
            // Untyped float literal: return unknown for context inference
            return typeSystem->getUnknownType();
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            // Check if this is the special ERR literal
            if (arg == "ERR") {
                // ERR literal: represents TBB error sentinel
                // Type will be resolved from context (tbb8/tbb16/tbb32/tbb64)
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

        // Standard undefined identifier error
        addError("Undefined identifier: '" + expr->name + "'", expr);
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
    
    // Context-aware literal typing for standard integers
    // If one operand is a literal int64 and the other is a smaller standard integer type,
    // check if the literal value fits in the smaller type. If so, use the smaller type
    // for the literal to avoid unnecessary widening.
    
    // Check if right operand is an int64 literal and left is a standard integer
    if (expr->right->type == ASTNode::NodeType::LITERAL &&
        isStandardIntType(leftType) &&
        rightType->toString() == "int64") {
        
        LiteralExpr* rightLiteral = static_cast<LiteralExpr*>(expr->right.get());
        if (std::holds_alternative<int64_t>(rightLiteral->value)) {
            int64_t value = std::get<int64_t>(rightLiteral->value);
            
            // Check if literal fits in left type - if so, use left type for the literal
            if (literalFitsInType(value, leftType)) {
                rightType = leftType;  // Treat literal as having the same type as left operand
            }
        }
    }
    
    // Check if left operand is an int64 literal and right is a standard integer
    if (expr->left->type == ASTNode::NodeType::LITERAL &&
        isStandardIntType(rightType) &&
        leftType->toString() == "int64") {
        
        LiteralExpr* leftLiteral = static_cast<LiteralExpr*>(expr->left.get());
        if (std::holds_alternative<int64_t>(leftLiteral->value)) {
            int64_t value = std::get<int64_t>(leftLiteral->value);
            
            // Check if literal fits in right type - if so, use right type for the literal
            if (literalFitsInType(value, rightType)) {
                leftType = rightType;  // Treat literal as having the same type as right operand
            }
        }
    }
    
    // Check operator validity for given types
    return checkBinaryOperator(expr->op.type, leftType, rightType);
}

// ============================================================================
// Binary Operator Type Checking
// ============================================================================

Type* TypeChecker::checkBinaryOperator(frontend::TokenType op, Type* leftType, Type* rightType) {
    using frontend::TokenType;
    
    // ========================================================================
    // Arithmetic Operators: +, -, *, /, %
    // ========================================================================
    if (op == TokenType::TOKEN_PLUS || op == TokenType::TOKEN_MINUS ||
        op == TokenType::TOKEN_STAR || op == TokenType::TOKEN_SLASH ||
        op == TokenType::TOKEN_PERCENT) {
        
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
        
        // Both operands must be numeric (int*, uint*, flt*, tbb*)
        PrimitiveType* leftPrim = dynamic_cast<PrimitiveType*>(leftType);
        PrimitiveType* rightPrim = dynamic_cast<PrimitiveType*>(rightType);
        
        if (!leftPrim || !rightPrim) {
            addError("Arithmetic operators require numeric types", 0, 0);
            return typeSystem->getErrorType();
        }
        
        // Check if types are numeric
        const std::string& leftName = leftPrim->getName();
        const std::string& rightName = rightPrim->getName();
        
        bool leftIsNumeric = (leftName.find("int") == 0 || leftName.find("uint") == 0 ||
                             leftName.find("flt") == 0 || leftName.find("tbb") == 0 ||
                             leftName.find("frac") == 0 || leftName.find("tfp") == 0 ||
                             leftName == "vec9" || leftName == "dvec9" ||
                             leftName == "trit" || leftName == "tryte" ||
                             leftName == "nit" || leftName == "nyte");
        bool rightIsNumeric = (rightName.find("int") == 0 || rightName.find("uint") == 0 ||
                              rightName.find("flt") == 0 || rightName.find("tbb") == 0 ||
                              rightName.find("frac") == 0 || rightName.find("tfp") == 0 ||
                              rightName == "vec9" || rightName == "dvec9" ||
                              rightName == "trit" || rightName == "tryte" ||
                              rightName == "nit" || rightName == "nyte");
        
        if (!leftIsNumeric || !rightIsNumeric) {
            addError("Arithmetic operators require numeric types, got '" + 
                    leftName + "' and '" + rightName + "'", 0, 0);
            return typeSystem->getErrorType();
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
            addError("Bitwise operators require unsigned integer types", 0, 0);
            return typeSystem->getErrorType();
        }
        
        const std::string& leftName = leftPrim->getName();
        const std::string& rightName = rightPrim->getName();
        
        // Check for unsigned prefix
        if (leftName.find("uint") != 0 || rightName.find("uint") != 0) {
            addError("Bitwise operators require unsigned types. Got '" + 
                    leftName + "' and '" + rightName + "'. Cast to unsigned (uint*) to perform bit manipulation.", 0, 0);
            return typeSystem->getErrorType();
        }
        
        // Result type is the common type (both must be same unsigned type)
        if (!leftType->equals(rightType)) {
            addError("Bitwise operators require same unsigned type on both sides", 0, 0);
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
        
        // Require compatible types
        if (!leftType->equals(rightType) && !canCoerce(leftType, rightType) && !canCoerce(rightType, leftType)) {
            addError("Cannot compare incompatible types: '" + 
                    leftType->toString() + "' and '" + rightType->toString() + "'", 0, 0);
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
                    "' to '" + leftType->toString() + "'", 0, 0);
            return typeSystem->getErrorType();
        }
        
        // Result type is left side type
        return leftType;
    }
    
    // Unknown operator
    addError("Unknown binary operator", 0, 0);
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
    return checkUnaryOperator(expr->op.type, operandType);
}

// ============================================================================
// Unary Operator Type Checking
// ============================================================================

Type* TypeChecker::checkUnaryOperator(frontend::TokenType op, Type* operandType) {
    using frontend::TokenType;
    
    PrimitiveType* primType = dynamic_cast<PrimitiveType*>(operandType);
    
    // ========================================================================
    // Arithmetic Negation: -
    // ========================================================================
    if (op == TokenType::TOKEN_MINUS) {
        // Require numeric type
        if (!primType) {
            addError("Unary minus requires numeric type, got '" + 
                    operandType->toString() + "'", 0, 0);
            return typeSystem->getErrorType();
        }
        
        const std::string& name = primType->getName();
        bool isNumeric = (name.find("int") == 0 || name.find("flt") == 0 || name.find("tbb") == 0 ||
                         name == "trit" || name == "tryte" || name == "nit" || name == "nyte" ||
                         name.find("frac") == 0 || name.find("tfp") == 0 ||
                         name == "vec9" || name == "dvec9");
        
        if (!isNumeric) {
            addError("Unary minus requires numeric type, got '" + name + "'", 0, 0);
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
                    operandType->toString() + "'. Use explicit comparison (e.g., x == 0) instead of truthiness.", 0, 0);
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
                    operandType->toString() + "'. Cast to unsigned (uint*) to perform bit manipulation.", 0, 0);
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
            addError("Dereference operator (<-) requires pointer type", 0, 0);
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
        addError("Unwrap operator (?) not yet implemented in type system", 0, 0);
        return typeSystem->getErrorType();
    }
    
    // Unknown operator
    addError("Unknown unary operator", 0, 0);
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
// Function Call Type Inference
// ============================================================================

Type* TypeChecker::inferCallExpr(CallExpr* expr) {
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
            // Returns void for now (in full impl would be 'never' type)
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
        if (idExpr->name == "@popcount64" || idExpr->name == "popcount64") {
            if (expr->arguments.size() != 1) {
                addError("@popcount64() requires exactly one argument (int64)", expr);
                return typeSystem->getErrorType();
            }
            return typeSystem->getPrimitiveType("int32");
        }
        
        // ====================================================================
        // STANDARD I/O BUILTINS
        // ====================================================================
        
        // Builtin: print() - accepts any single argument, returns void
        if (idExpr->name == "print") {
            if (expr->arguments.size() != 1) {
                addError("print() requires exactly one argument", expr);
                return typeSystem->getErrorType();
            }
            // Infer argument type to validate it
            Type* argType = inferType(expr->arguments[0].get());
            if (argType->getKind() == TypeKind::ERROR) {
                return typeSystem->getErrorType();
            }
            // print() returns i32 (printf's return value)
            return typeSystem->getPrimitiveType("int32");
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
        
        // Use the specialized function's return type
        // TODO: Parse return type string to Type*
        // For now, return UnknownType until we implement type parsing
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
            
            // Check if argument type is assignable to parameter type
            if (!argType->isAssignableTo(paramTypes[i])) {
                addError("Argument " + std::to_string(i + 1) + " has type '" + 
                        argType->toString() + "', but function expects '" + 
                        paramTypes[i]->toString() + "'", expr->arguments[i].get());
                // Continue checking other arguments to report all errors
            }
        }
        
        // Return the function's return type
        return funcType->getReturnType();
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
    
    // Arrays are represented as pointer types
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
    if (objectType->getKind() == TypeKind::RESULT) {
        ResultType* resultType = static_cast<ResultType*>(objectType);
        
        // .is_error -> bool
        if (expr->member == "is_error") {
            return typeSystem->getPrimitiveType("bool");
        }
        
        // .value -> T (the wrapped value type)
        if (expr->member == "value") {
            return resultType->getValueType();
        }
        
        // .error -> error pointer (represented as int64 for now)
        // TODO: Create proper Error type when error handling is expanded
        if (expr->member == "error") {
            // Return pointer to error (void* in C, int64 in Aria IR)
            return typeSystem->getPrimitiveType("int64");
        }
        
        // Invalid member
        addError("result<T> has no member '" + expr->member + 
                "' (valid: is_error, value, error)", expr);
        return typeSystem->getErrorType();
    }
    
    // For safe navigation (?.), the object can be null/NIL
    // In that case, the result type should be the same as normal access
    // but the IR generation will handle the null check
    // TODO: When optional types are implemented, safe navigation should return optional<T>
    
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
    
    // For now, without full result type implementation, we require that
    // the result and default value have compatible types
    Type* commonType = findCommonType(resultType, defaultType);
    
    if (commonType->getKind() == TypeKind::ERROR) {
        addError("Unwrap operator (?) requires compatible types: result type '" + 
                resultType->toString() + "' and default value type '" + 
                defaultType->toString() + "'", expr);
        return typeSystem->getErrorType();
    }
    
    // The unwrap expression returns the common type of result and default
    return commonType;
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
        // Lookup struct type
        Type* structType = typeSystem->getStructType(expr->type_name);
        
        if (!structType) {
            addError("Unknown struct type: '" + expr->type_name + "'", expr);
            return typeSystem->getErrorType();
        }
        
        // TODO: When struct types are fully implemented, validate field types here
        // For now, just return the struct type
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
    
    // Return pointer to element type (arrays decay to pointers)
    return typeSystem->getPointerType(elementType);
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
            
            // Try primitive type
            type = typeSystem->getPrimitiveType(simpleType->typeName);
            if (type && type->getKind() != TypeKind::ERROR) {
                return type;
            }
            
            addError("Unknown type: '" + simpleType->typeName + "'", typeNode);
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
            
            // For now, return the element type with a note that it's an array
            // TODO: Create proper ArrayType in TypeSystem
            // Temporary: Use pointer type to represent arrays (common in many languages)
            return typeSystem->getPointerType(elementType);
        }
        
        case ASTNode::NodeType::POINTER_TYPE: {
            // Pointer type: int32@, string@, etc.
            aria::PointerType* ptrType = static_cast<aria::PointerType*>(typeNode);
            
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
            if (genericType->baseName == "Result" || genericType->baseName == "array") {
                // Built-in generic types - handle specially
                // For now, just return a placeholder struct type
                // The actual implementation will be in stdlib once fully integrated
                
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
                if (genericType->baseName == "Result") {
                    // result<T> has: bool is_error, T val, string err
                    fields.push_back({"is_error", typeSystem->getPrimitiveType("bool"), 0});
                    fields.push_back({"val", resolvedTypeArgs[0], 8});
                    fields.push_back({"err", typeSystem->getPrimitiveType("string"), 16});
                } else if (genericType->baseName == "array") {
                    // array<T> has: T* data, int64 length, int64 capacity
                    Type* elemType = resolvedTypeArgs[0];
                    Type* ptrType = typeSystem->getPointerType(elemType);
                    fields.push_back({"data", ptrType, 0});
                    fields.push_back({"length", typeSystem->getPrimitiveType("int64"), 8});
                    fields.push_back({"capacity", typeSystem->getPrimitiveType("int64"), 16});
                }
                
                return typeSystem->createStructType(mangledName, fields);
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
        
        default:
            addError("Unsupported type node in type resolution", typeNode);
            return typeSystem->getErrorType();
    }
}

// ============================================================================
// Variable Declaration Type Checking
// ============================================================================

void TypeChecker::checkVarDecl(VarDeclStmt* stmt) {
    Type* declaredType = nullptr;
    
    // Handle new typeNode or legacy typeName
    if (stmt->typeNode) {
        // Resolve type from AST node (supports arrays, pointers, etc.)
        declaredType = resolveTypeNode(stmt->typeNode.get());
        if (!declaredType || declaredType->getKind() == TypeKind::ERROR) {
            addError("Cannot resolve type in variable declaration", stmt);
            return;
        }
        
        // CRITICAL: Update typeName to resolved type name for IR generation
        // For generic structs, this will be the mangled name (_Aria_M_Box_<hash>_int64)
        // For other types, this will be the canonical name (int64, string, etc.)
        stmt->typeName = declaredType->toString();
    } else {
        // Legacy path: Get declared type from typeName string
        // Try struct first to avoid auto-creating primitive types
        declaredType = typeSystem->getStructType(stmt->typeName);
        
        if (!declaredType) {
            // Try primitive type
            declaredType = typeSystem->getPrimitiveType(stmt->typeName);
            
            if (!declaredType || declaredType->getKind() == TypeKind::ERROR) {
                addError("Unknown type: '" + stmt->typeName + "'", stmt);
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
        
        // TBB Type Validation (Phase 3.2.4)
        // Special handling for integer literals assigned to TBB types
        bool tbbLiteralAssignment = false;
        if (isTBBType(declaredType) && stmt->initializer->type == ASTNode::NodeType::LITERAL) {
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
        if (isBalancedType(declaredType) && stmt->initializer->type == ASTNode::NodeType::LITERAL) {
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
        if (isStandardIntType(declaredType) && stmt->initializer->type == ASTNode::NodeType::LITERAL) {
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
        
        // Check if initializer type is assignable to declared type
        // Skip this check if we handled TBB, ERR, balanced, standard int, or high-precision literal assignment above
        if (!tbbLiteralAssignment && !errLiteralAssignment && !balancedLiteralAssignment && !standardIntLiteralAssignment && !highPrecisionLiteralAssignment) {
            // Special case: Allow T to be initialized to T? (wrapping)
            bool optionalWrapping = false;
            if (declaredType->getKind() == TypeKind::OPTIONAL) {
                const OptionalType* declaredOptional = static_cast<const OptionalType*>(declaredType);
                if (initType->isAssignableTo(declaredOptional->getWrappedType())) {
                    optionalWrapping = true;
                }
            }
            
            if (!initType->isAssignableTo(declaredType) && !canCoerce(initType, declaredType) && !optionalWrapping) {
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
    
    // If we evaluated a const value, store it in the symbol
    if (evaluatedConstValue) {
        Symbol* sym = symbolTable->resolveSymbol(stmt->varName);
        if (sym) {
            sym->setComptimeValue(evaluatedConstValue);
        }
    }
}

// ============================================================================
// Function Declaration Type Checking
// ============================================================================

void TypeChecker::checkFuncDecl(FuncDeclStmt* stmt) {
    // Resolve return type from the type node
    Type* returnType = resolveTypeNode(stmt->returnType.get());
    
    if (!returnType) {
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
        std::string returnTypeName = returnType->toString();
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

    // ARIA-026 FIX: FFI Safety - Ban string return types from extern functions
    if (!stmt->body && returnType->toString() == "str") {  // No body = extern function
        addError("FFI Safety Violation: Cannot return 'str' from extern function '" + 
                 stmt->funcName + "'. Aria strings are fat pointers {data, len} incompatible " +
                 "with C char*. Use 'wild int8*' for C strings and convert with string_from_cstr().", 
                 stmt);
    }

    // Set current function return type for return statement checking
    Type* previousReturnType = currentFunctionReturnType;
    currentFunctionReturnType = returnType;
    
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
            symbolTable->defineSymbol(paramNode->paramName, SymbolKind::VARIABLE,
                                     paramType, param->line, param->column);
        }
    }
    
    // Check function body (this is where generic calls get analyzed!)
    if (stmt->body) {
        checkStatement(stmt->body.get());
    }
    
    // Restore previous function return type
    currentFunctionReturnType = previousReturnType;
    
    // Build parameter types for function type
    std::vector<Type*> paramTypes;
    for (const auto& param : stmt->parameters) {
        if (param->type == ASTNode::NodeType::PARAMETER) {
            ParameterNode* paramNode = static_cast<ParameterNode*>(param.get());
            Type* paramType = resolveTypeNode(paramNode->typeNode.get());
            if (paramType && paramType->getKind() != TypeKind::ERROR) {
                paramTypes.push_back(paramType);
            } else {
                // Use void as fallback for error types
                paramTypes.push_back(typeSystem->getPrimitiveType("void"));
            }
        }
    }
    
    // Create proper function type with parameters and return type
    Type* funcType = new FunctionType(paramTypes, returnType, stmt->isAsync, false);
    
    // Define function symbol in symbol table with complete function type
    Symbol* funcSymbol = symbolTable->defineSymbol(stmt->funcName, SymbolKind::FUNCTION,
                                                    funcType, stmt->line, stmt->column);
    if (funcSymbol) {
        funcSymbol->setFuncDecl(stmt);  // Store AST for generic resolution and CTFE
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
            addError("Generic struct '" + stmt->structName + "' is already defined", stmt);
            return;
        }
        genericStructRegistry[stmt->structName] = stmt;
        return;  // Don't register with TypeSystem yet
    }
    
    // Non-generic struct - check if already defined
    Type* existingStruct = typeSystem->getStructType(stmt->structName);
    if (existingStruct) {
        addError("Struct '" + stmt->structName + "' is already defined", stmt);
        return;
    }
    
    // Note: We don't check primitiveCache because getPrimitiveType auto-creates types
    // This is fine - struct names should just avoid conflicting with known primitives
    
    // Extract and validate field information
    std::vector<StructType::Field> fields;
    for (const auto& field : stmt->fields) {
        if (field->type == ASTNode::NodeType::VAR_DECL) {
            VarDeclStmt* fieldDecl = static_cast<VarDeclStmt*>(field.get());
            
            // Look up field type (check struct types first to avoid auto-creating primitives)
            Type* fieldType = typeSystem->getStructType(fieldDecl->typeName);
            if (!fieldType) {
                fieldType = typeSystem->getPrimitiveType(fieldDecl->typeName);
            }
            
            if (!fieldType || fieldType->getKind() == TypeKind::ERROR) {
                addError("Unknown field type '" + fieldDecl->typeName + 
                        "' in struct '" + stmt->structName + "'", field.get());
                // Continue checking other fields
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
    // Check if enum name already exists
    // Note: For now, we just register enum variants as constants in the symbol table
    // Full enum type support can be added later
    
    // Register each enum variant as a constant
    for (const auto& [variantName, variantValue] : stmt->variants) {
        std::string fullName = stmt->enumName + "." + variantName;
        
        // Check if this variant name already exists in the enum
        if (symbolTable->isDefined(fullName)) {
            addError("Enum variant '" + variantName + "' is already defined in enum '" + 
                    stmt->enumName + "'", stmt);
            continue;
        }
        
        // For now, register the variant as an int64 constant in the symbol table
        // This allows code like: int32:x = MyEnum.VARIANT;
        // TODO: Create a proper EnumType when type system is extended
        symbolTable->defineSymbol(fullName, SymbolKind::VARIABLE, typeSystem->getPrimitiveType("int64"));
    }
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
                        Type* ptype = typeSystem->getPrimitiveType(paramTypeName);
                        if (!ptype) ptype = typeSystem->getPrimitiveType("int64");  // fallback
                        paramTypes.push_back(ptype);
                    }
                }
            }

            std::string retTypeName = funcDecl->returnType ? funcDecl->returnType->toString() : "void";
            Type* retType = typeSystem->getPrimitiveType(retTypeName);
            if (!retType) retType = typeSystem->getPrimitiveType("void");

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
    
    // Left side must be an lvalue (identifier, index, member access)
    if (expr->left->type != ASTNode::NodeType::IDENTIFIER &&
        expr->left->type != ASTNode::NodeType::INDEX &&
        expr->left->type != ASTNode::NodeType::MEMBER_ACCESS &&
        expr->left->type != ASTNode::NodeType::POINTER_MEMBER) {
        addError("Left side of assignment must be a variable, array element, or member", expr->left.get());
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
        addError("void function cannot return a value", stmt);
        return;
    }
    
    // Case 2: non-void function without return value
    if (!isVoidFunction && !stmt->value) {
        addError("Non-void function must return a value of type '" + 
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
            isStandardIntType(currentFunctionReturnType) &&
            returnType->toString() == "int64") {
            
            LiteralExpr* literal = static_cast<LiteralExpr*>(stmt->value.get());
            if (std::holds_alternative<int64_t>(literal->value)) {
                int64_t value = std::get<int64_t>(literal->value);
                if (literalFitsInType(value, currentFunctionReturnType)) {
                    // Treat literal as having the function's return type
                    return;  // Success, literal fits
                }
            }
        }
        
        if (!returnType->isAssignableTo(currentFunctionReturnType) && 
            !canCoerce(returnType, currentFunctionReturnType)) {
            addError("Return type '" + returnType->toString() + 
                    "' does not match function return type '" + 
                    currentFunctionReturnType->toString() + "'", stmt);
        }
    }
}

// ============================================================================
// Pass/Fail Statement Type Checking (Result Types)
// ============================================================================

void TypeChecker::checkPassStmt(PassStmt* stmt) {
    // Check if we're inside a function
    if (!currentFunctionReturnType) {
        addError("pass statement outside of function", stmt);
        return;
    }
    
    // Get the primitive type "void" for comparison
    Type* voidType = typeSystem->getPrimitiveType("void");
    bool isVoidFunction = currentFunctionReturnType->equals(voidType);
    
    // void function cannot use pass
    if (isVoidFunction) {
        addError("void function cannot use pass statement", stmt);
        return;
    }
    
    // Infer type of the pass value
    Type* valueType = inferType(stmt->value.get());
    
    if (valueType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // Check if value type matches function return type
    if (!valueType->isAssignableTo(currentFunctionReturnType) && 
        !canCoerce(valueType, currentFunctionReturnType)) {
        addError("Pass value type '" + valueType->toString() + 
                "' does not match function return type '" + 
                currentFunctionReturnType->toString() + "'", stmt);
    }
}

void TypeChecker::checkFailStmt(FailStmt* stmt) {
    // Check if we're inside a function
    if (!currentFunctionReturnType) {
        addError("fail statement outside of function", stmt);
        return;
    }
    
    // Infer type of the error code
    Type* errorType = inferType(stmt->errorCode.get());
    
    if (errorType->getKind() == TypeKind::ERROR) {
        return;
    }
    
    // For now, just ensure error code is an integer type
    // TODO: When full result types are implemented, this will be more sophisticated
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
    
    // Check then branch
    checkStatement(stmt->thenBranch.get());
    
    // Check else branch if present
    if (stmt->elseBranch) {
        checkStatement(stmt->elseBranch.get());
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
    
    // Check body
    checkStatement(stmt->body.get());
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
        
        // Check body with iterator in scope
        checkStatement(stmt->body.get());
        
        // Exit scope
        symbolTable->exitScope();
        return;
    }
    
    // C-style for loop
    // Check initializer if present
    if (stmt->initializer) {
        checkStatement(stmt->initializer.get());
    }
    
    // Check condition if present
    if (stmt->condition) {
        Type* condType = inferType(stmt->condition.get());
        
        if (condType->getKind() != TypeKind::ERROR) {
            PrimitiveType* condPrim = dynamic_cast<PrimitiveType*>(condType);
            if (!condPrim || condPrim->getName() != "bool") {
                addError("for condition must be 'bool' type, got '" + condType->toString() + 
                        "'. Use explicit comparison (e.g., i < 10) instead of truthiness.", stmt->condition.get());
            }
        }
    }
    
    // Check update if present (just infer type, any expression is valid)
    if (stmt->update) {
        inferType(stmt->update.get());
    }
    
    // Check body
    checkStatement(stmt->body.get());
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
        // trit must be exactly -1, 0, or 1
        if (value != -1 && value != 0 && value != 1) {
            addError("trit value must be -1, 0, or 1, got " + std::to_string(value), node);
        }
    } else if (isNitType(type)) {
        // nit must be -4, -3, -2, -1, 0, 1, 2, 3, or 4
        if (value < -4 || value > 4) {
            addError("nit value must be in range [-4, 4], got " + std::to_string(value), node);
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
    
    return name == "int8" || name == "int16" || name == "int32" || name == "int64" ||
           name == "uint8" || name == "uint16" || name == "uint32" || name == "uint64";
}

bool TypeChecker::literalFitsInType(int64_t value, Type* type) const {
    if (!isStandardIntType(type)) {
        return false;
    }
    
    PrimitiveType* primType = static_cast<PrimitiveType*>(type);
    const std::string& typeName = primType->getName();
    
    // Determine range based on type name
    if (typeName == "int8") {
        return value >= -128 && value <= 127;
    } else if (typeName == "int16") {
        return value >= -32768 && value <= 32767;
    } else if (typeName == "int32") {
        return value >= -2147483648LL && value <= 2147483647LL;
    } else if (typeName == "int64") {
        return true;  // int64 can hold any int64_t value
    } else if (typeName == "uint8") {
        return value >= 0 && value <= 255;
    } else if (typeName == "uint16") {
        return value >= 0 && value <= 65535;
    } else if (typeName == "uint32") {
        return value >= 0 && value <= 4294967295LL;
    } else if (typeName == "uint64") {
        return value >= 0;  // uint64 can hold any non-negative int64_t value
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
    if (typeName == "int8") {
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
        if (value < 0) {
            addError("Cannot assign negative value " + std::to_string(value) + " to unsigned type " + typeName, node);
            return false;
        }
        // uint64 can hold any non-negative int64_t value
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
    
    // Load the module using ModuleLoader
    LoadedModule* module = moduleLoader->loadModule(logicalPath, currentModulePath);
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
            // Temporarily export all top-level function declarations
            // (Until we implement `pub` keyword parsing)
            for (const auto& decl : module->ast->declarations) {
                if (decl->type == ASTNode::NodeType::FUNC_DECL) {
                    FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(decl.get());
                    
                    // Build the function type from the declaration
                    // First, resolve parameter types
                    std::vector<Type*> paramTypes;
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
                    
                    // Resolve return type
                    Type* returnType = resolveTypeNode(funcDecl->returnType.get());
                    if (!returnType || returnType->getKind() == TypeKind::ERROR) {
                        returnType = typeSystem->getPrimitiveType("void");
                    }
                    
                    // Create the function type
                    Type* funcType = new FunctionType(paramTypes, returnType, funcDecl->isAsync, false);
                    
                    // Create a new symbol for export
                    Symbol* funcSym = new Symbol(funcDecl->funcName, SymbolKind::FUNCTION, funcType, nullptr, funcDecl->line, funcDecl->column);
                    funcSym->funcDecl = funcDecl;
                    
                    // Export the function (PUBLIC visibility by default for now)
                    module->moduleInfo->exportSymbol(funcDecl->funcName, funcSym, Visibility::PUBLIC);
                }
            }
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
        
        // Check for name collision
        if (symbolTable->isDefined(name)) {
            addError("Symbol '" + name + "' already defined in current scope (wildcard import conflict)", stmt);
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
    
    // Type-check all case bodies
    for (const auto& caseNode : stmt->cases) {
        PickCase* pickCase = static_cast<PickCase*>(caseNode.get());
        if (pickCase->body) {
            checkStatement(pickCase->body.get());
        }
    }
}

} // namespace sema
} // namespace aria
