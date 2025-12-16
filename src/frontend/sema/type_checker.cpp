#include "frontend/sema/type_checker.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include <sstream>
#include <variant>

namespace aria {
namespace sema {

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
        
        default:
            addError("Type inference not implemented for node type: " + 
                    ASTNode::nodeTypeToString(expr->type), expr);
            return typeSystem->getErrorType();
    }
}

// ============================================================================
// Literal Type Inference
// ============================================================================

Type* TypeChecker::inferLiteral(LiteralExpr* expr) {
    // Use std::visit to handle variant
    return std::visit([this](auto&& arg) -> Type* {
        using T = std::decay_t<decltype(arg)>;
        
        if constexpr (std::is_same_v<T, int64_t>) {
            // Integer literal: default to int64
            return typeSystem->getPrimitiveType("int64");
        }
        else if constexpr (std::is_same_v<T, double>) {
            // Float literal: default to flt64 (double precision)
            return typeSystem->getPrimitiveType("flt64");
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            // String literal
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
// Identifier Type Inference
// ============================================================================

Type* TypeChecker::inferIdentifier(IdentifierExpr* expr) {
    // Lookup symbol in symbol table
    Symbol* symbol = symbolTable->lookupSymbol(expr->name);
    
    if (!symbol) {
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
                             leftName.find("flt") == 0 || leftName.find("tbb") == 0);
        bool rightIsNumeric = (rightName.find("int") == 0 || rightName.find("uint") == 0 ||
                              rightName.find("flt") == 0 || rightName.find("tbb") == 0);
        
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
        bool isNumeric = (name.find("int") == 0 || name.find("flt") == 0 || name.find("tbb") == 0);
        
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
        // Create pointer type
        // TODO: Implement pointer type creation in Phase 3.2.1 enhancements
        // For now, return ErrorType with message
        addError("Address-of operator (@) not yet implemented in type system", 0, 0);
        return typeSystem->getErrorType();
    }
    
    // ========================================================================
    // Pin: #
    // ========================================================================
    if (op == TokenType::TOKEN_HASH) {
        // Pin GC object to get wild pointer
        // TODO: Check that operand is GC object type
        addError("Pin operator (#) not yet implemented in type system", 0, 0);
        return typeSystem->getErrorType();
    }
    
    // ========================================================================
    // Borrow/Iterate: $
    // ========================================================================
    if (op == TokenType::TOKEN_DOLLAR) {
        // Borrow or iterate over collection
        // TODO: Check that operand is array or iterator type
        addError("Borrow operator ($) not yet implemented in type system", 0, 0);
        return typeSystem->getErrorType();
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
// Function Call Type Inference
// ============================================================================

Type* TypeChecker::inferCallExpr(CallExpr* expr) {
    // Infer callee type (should be function type or callable object)
    Type* calleeType = inferType(expr->callee.get());
    
    if (calleeType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // TODO: Check that callee is a function type
    // TODO: Check argument count and types match parameters
    // For now, return UnknownType
    addError("Function call type checking not yet fully implemented", expr);
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
    
    // TODO: Check that arrayType is array type, return element type
    // For now, return UnknownType
    addError("Array indexing type checking not yet fully implemented", expr);
    return typeSystem->getUnknownType();
}

// ============================================================================
// Member Access Type Inference
// ============================================================================

Type* TypeChecker::inferMemberAccessExpr(MemberAccessExpr* expr) {
    // Infer object type
    Type* objectType = inferType(expr->object.get());
    
    if (objectType->getKind() == TypeKind::ERROR) {
        return typeSystem->getErrorType();
    }
    
    // TODO: Check that object is struct/union type
    // TODO: Lookup member in struct/union and return its type
    // For now, return UnknownType
    addError("Member access type checking not yet fully implemented", expr);
    return typeSystem->getUnknownType();
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
    // Numeric Widening: int8 → int16 → int32 → int64 → int128 → int256 → int512
    // ========================================================================
    
    // Extract bit width from type name (e.g., "int32" → 32)
    auto extractBitWidth = [](const std::string& name) -> int {
        if (name.find("int") == 0) {
            size_t pos = (name[0] == 'u') ? 4 : 3;  // "uint" or "int"
            return std::stoi(name.substr(pos));
        }
        if (name.find("flt") == 0) {
            return std::stoi(name.substr(3));
        }
        if (name.find("tbb") == 0) {
            return std::stoi(name.substr(3));
        }
        return 0;
    };
    
    // Signed integer widening
    if (fromName.find("int") == 0 && toName.find("int") == 0) {
        int fromWidth = extractBitWidth(fromName);
        int toWidth = extractBitWidth(toName);
        return fromWidth < toWidth;  // Allow widening only
    }
    
    // Unsigned integer widening
    if (fromName.find("uint") == 0 && toName.find("uint") == 0) {
        int fromWidth = extractBitWidth(fromName);
        int toWidth = extractBitWidth(toName);
        return fromWidth < toWidth;  // Allow widening only
    }
    
    // TBB widening (preserves error semantics)
    if (fromName.find("tbb") == 0 && toName.find("tbb") == 0) {
        int fromWidth = extractBitWidth(fromName);
        int toWidth = extractBitWidth(toName);
        return fromWidth < toWidth;  // Allow widening only
    }
    
    // Float widening: flt32 → flt64
    if (fromName.find("flt") == 0 && toName.find("flt") == 0) {
        int fromWidth = extractBitWidth(fromName);
        int toWidth = extractBitWidth(toName);
        return fromWidth < toWidth;  // Allow widening only
    }
    
    // ========================================================================
    // Integer to Float Promotion
    // ========================================================================
    if (fromName.find("int") == 0 && toName.find("flt") == 0) {
        // Allow int → float promotion (e.g., int32 → flt32, int64 → flt64)
        // Note: This can lose precision for large integers
        return true;
    }
    
    // ========================================================================
    // Disallowed Coercions (Explicit Cast Required)
    // ========================================================================
    
    // No narrowing (int32 → int8)
    // No float to int (flt32 → int32)
    // No standard ↔ TBB (int32 ↔ tbb32)
    // No signed ↔ unsigned (int32 ↔ uint32)
    
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
        
        case ASTNode::NodeType::RETURN:
            checkReturnStmt(static_cast<ReturnStmt*>(stmt));
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
        
        default:
            // Other statement types not yet implemented
            break;
    }
}

// ============================================================================
// Variable Declaration Type Checking
// ============================================================================

void TypeChecker::checkVarDecl(VarDeclStmt* stmt) {
    // Get declared type
    Type* declaredType = typeSystem->getPrimitiveType(stmt->typeName);
    
    if (!declaredType || declaredType->getKind() == TypeKind::ERROR) {
        addError("Unknown type: '" + stmt->typeName + "'", stmt);
        return;
    }
    
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
        
        // Check if initializer type is assignable to declared type
        if (!initType->isAssignableTo(declaredType) && !canCoerce(initType, declaredType)) {
            addError("Cannot initialize variable '" + stmt->varName + 
                    "' of type '" + declaredType->toString() + 
                    "' with value of type '" + initType->toString() + "'", stmt);
            return;
        }
    }
    
    // Define symbol in symbol table
    symbolTable->defineSymbol(stmt->varName, 
                             stmt->isConst ? SymbolKind::CONSTANT : SymbolKind::VARIABLE,
                             declaredType, 
                             stmt->line, 
                             stmt->column);
}

// ============================================================================
// Assignment Type Checking
// ============================================================================

void TypeChecker::checkAssignment(BinaryExpr* expr) {
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
    
    // Check type compatibility
    if (!rightType->isAssignableTo(leftType) && !canCoerce(rightType, leftType)) {
        addError("Cannot assign value of type '" + rightType->toString() + 
                "' to variable of type '" + leftType->toString() + "'", expr);
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
        
        if (!returnType->isAssignableTo(currentFunctionReturnType) && 
            !canCoerce(returnType, currentFunctionReturnType)) {
            addError("Return type '" + returnType->toString() + 
                    "' does not match function return type '" + 
                    currentFunctionReturnType->toString() + "'", stmt);
        }
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
    // Just infer the expression type
    // Special case: check if it's an assignment
    if (stmt->expression->type == ASTNode::NodeType::BINARY_OP) {
        BinaryExpr* binExpr = static_cast<BinaryExpr*>(stmt->expression.get());
        if (binExpr->op.type == frontend::TokenType::TOKEN_EQUAL) {
            checkAssignment(binExpr);
            return;
        }
    }
    
    // Otherwise just infer type (to check for errors)
    inferType(stmt->expression.get());
}

} // namespace sema
} // namespace aria
