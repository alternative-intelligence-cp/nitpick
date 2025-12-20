#include "frontend/sema/type_checker.h"
#include "frontend/sema/generic_resolver.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/type.h"  // For ArrayType, SimpleType, etc.
#include <sstream>
#include <variant>

namespace aria {
namespace sema {

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
        
        case ASTNode::NodeType::OBJECT_LITERAL:
            return inferObjectLiteral(static_cast<ObjectLiteralExpr*>(expr));
        
        case ASTNode::NodeType::ARRAY_LITERAL:
            return inferArrayLiteral(static_cast<ArrayLiteralExpr*>(expr));
        
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
    // TODO: Check argument count and types match parameters
    // For now, return UnknownType
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
    
    // Arrays are represented as pointer types
    // Extract element type from pointer
    if (arrayType->getKind() == TypeKind::POINTER) {
        PointerType* ptrType = static_cast<PointerType*>(arrayType);
        return ptrType->getPointeeType();
    }
    
    // Not an array/pointer type
    addError("Cannot index non-array type '" + arrayType->toString() + "'", expr);
    return typeSystem->getErrorType();
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
    
    // Handle struct member access
    if (objectType->getKind() == TypeKind::STRUCT) {
        StructType* structType = static_cast<StructType*>(objectType);
        
        // Look up member in struct fields
        for (const auto& field : structType->getFields()) {
            if (field.name == expr->member) {
                return field.type;
            }
        }
        
        // Member not found
        addError("Struct '" + structType->getName() + "' has no member named '" + 
                expr->member + "'", expr);
        return typeSystem->getErrorType();
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
    
    // TBB ↔ Standard Integer: FORBIDDEN (Phase 3.2.4)
    // TBB types have error sentinels and sticky error semantics
    // Standard integers use modular arithmetic
    // Mixing them requires explicit conversion
    bool fromIsTBB = (fromName.find("tbb") == 0);
    bool toIsTBB = (toName.find("tbb") == 0);
    bool fromIsStdInt = (fromName.find("int") == 0 || fromName.find("uint") == 0);
    bool toIsStdInt = (toName.find("int") == 0 || toName.find("uint") == 0);
    
    if ((fromIsTBB && toIsStdInt) || (fromIsStdInt && toIsTBB)) {
        return false;  // Explicit cast required
    }
    
    // Balanced ↔ Standard Integer: FORBIDDEN (Phase 3.2.5)
    // Balanced types (trit, tryte, nit, nyte) use symmetric digit sets
    // Standard integers use modular arithmetic
    // Mixing them requires explicit conversion
    bool fromIsBalanced = (fromName == "trit" || fromName == "tryte" || 
                          fromName == "nit" || fromName == "nyte");
    bool toIsBalanced = (toName == "trit" || toName == "tryte" || 
                        toName == "nit" || toName == "nyte");
    
    if ((fromIsBalanced && toIsStdInt) || (fromIsStdInt && toIsBalanced)) {
        return false;  // Explicit cast required
    }
    
    // Balanced ↔ TBB: FORBIDDEN (Phase 3.2.5)
    // Different semantic models
    if ((fromIsBalanced && toIsTBB) || (fromIsTBB && toIsBalanced)) {
        return false;  // Explicit cast required
    }
    
    // No narrowing (int32 → int8)
    // No float to int (flt32 → int32)
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
        
        case ASTNode::NodeType::FUNC_DECL:
            checkFuncDecl(static_cast<FuncDeclStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::STRUCT_DECL:
            checkStructDecl(static_cast<StructDeclStmt*>(stmt));
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
        // Skip this check if we handled TBB, balanced, or standard int literal assignment above
        if (!tbbLiteralAssignment && !balancedLiteralAssignment && !standardIntLiteralAssignment) {
            if (!initType->isAssignableTo(declaredType) && !canCoerce(initType, declaredType)) {
                addError("Cannot initialize variable '" + stmt->varName + 
                        "' of type '" + declaredType->toString() + 
                        "' with value of type '" + initType->toString() + "'", stmt);
                return;
            }
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
// Function Declaration Type Checking
// ============================================================================

void TypeChecker::checkFuncDecl(FuncDeclStmt* stmt) {
    // Get return type (try struct first to avoid auto-creating primitive types)
    Type* returnType = typeSystem->getStructType(stmt->returnType);
    
    if (!returnType) {
        // Try primitive type
        returnType = typeSystem->getPrimitiveType(stmt->returnType);
        
        if (!returnType || returnType->getKind() == TypeKind::ERROR) {
            addError("Unknown return type: '" + stmt->returnType + "'", stmt);
            return;
        }
    }
    
    // Set current function return type for return statement checking
    Type* previousReturnType = currentFunctionReturnType;
    currentFunctionReturnType = returnType;
    
    // Check parameters
    for (const auto& param : stmt->parameters) {
        if (param->type == ASTNode::NodeType::PARAMETER) {
            ParameterNode* paramNode = static_cast<ParameterNode*>(param.get());
            Type* paramType = typeSystem->getStructType(paramNode->typeName);
            
            if (!paramType) {
                // Try primitive type
                paramType = typeSystem->getPrimitiveType(paramNode->typeName);
                
                if (!paramType || paramType->getKind() == TypeKind::ERROR) {
                    addError("Unknown parameter type: '" + paramNode->typeName + "'", param.get());
                    continue;
                }
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
    
    // Define function symbol in symbol table with function declaration
    // Note: Function type creation would need more work for full signature
    Symbol* funcSymbol = symbolTable->defineSymbol(stmt->funcName, SymbolKind::FUNCTION,
                                                    returnType, stmt->line, stmt->column);
    if (funcSymbol) {
        funcSymbol->setFuncDecl(stmt);  // Store AST for generic resolution and CTFE
    }
}

// ============================================================================
// Struct Declaration Type Checking
// ============================================================================

void TypeChecker::checkStructDecl(StructDeclStmt* stmt) {
    // Check if already defined as a struct
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
        if (!rightType->isAssignableTo(leftType) && !canCoerce(rightType, leftType)) {
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
    // Detailed module resolution is handled by module_resolver
    
    if (stmt->path.empty()) {
        addError("Empty module path in use statement", stmt);
        return;
    }
    
    // For file path imports, validate the path format
    if (stmt->isFilePath) {
        const std::string& filePath = stmt->path[0];
        
        // Check for valid file extensions
        if (!filePath.empty()) {
            // File paths should end with .aria
            if (filePath.length() > 5) {
                std::string ext = filePath.substr(filePath.length() - 5);
                if (ext != ".aria") {
                    // Allow paths without extensions (will be resolved by module system)
                    // This is just a warning, not an error
                }
            }
        }
    }
    
    // Validate selective imports and wildcards
    if (stmt->isWildcard && !stmt->items.empty()) {
        addError("Cannot have both wildcard (*) and selective imports in use statement", stmt);
        return;
    }
    
    // Note: Actual symbol resolution and import validation happens during
    // symbol table construction and module linking phases
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

} // namespace sema
} // namespace aria
