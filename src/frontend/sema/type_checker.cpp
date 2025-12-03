/**
 * src/frontend/sema/type_checker.cpp
 *
 * Aria Type Checker Implementation
 * Version: 0.0.6
 */

#include "type_checker.h"
#include "types.h"
#include "../ast/expr.h"
#include "../ast/stmt.h"
#include "../ast/control_flow.h"
#include "../ast/defer.h"
#include "../ast/loops.h"

namespace aria {
namespace sema {

// Helper: Parse type from string
std::shared_ptr<Type> parseType(const std::string& type_str) {
    if (type_str == "void") return makeVoidType();
    if (type_str == "bool") return makeBoolType();
    if (type_str == "int8") return makeIntType(8);
    if (type_str == "int16") return makeIntType(16);
    if (type_str == "int32") return makeIntType(32);
    if (type_str == "int64") return makeIntType(64);
    if (type_str == "int128") return makeIntType(128);
    if (type_str == "int256") return makeIntType(256);
    if (type_str == "int512") return makeIntType(512);
    if (type_str == "flt32") return makeFloatType(32);
    if (type_str == "flt64") return makeFloatType(64);
    if (type_str == "string") return makeStringType();
    if (type_str == "dyn") return makeDynType();

    // Default to int64 for unknown types (for now)
    return makeIntType(64);
}

// Visit VarExpr (variable reference)
void TypeChecker::visit(frontend::VarExpr* node) {
    Symbol* sym = symbols->lookup(node->name);
    if (!sym) {
        addError("Undefined variable: " + node->name);
        current_expr_type = makeErrorType();
        return;
    }

    current_expr_type = sym->type;
}

// Visit IntLiteral
void TypeChecker::visit(frontend::IntLiteral* node) {
    // Integer literals default to int64
    current_expr_type = makeIntType(64);
}

// Visit BoolLiteral
void TypeChecker::visit(frontend::BoolLiteral* node) {
    current_expr_type = makeBoolType();
}

// Visit StringLiteral
void TypeChecker::visit(frontend::StringLiteral* node) {
    current_expr_type = makeStringType();
}

// Visit TemplateString
void TypeChecker::visit(frontend::TemplateString* node) {
    // Template strings always evaluate to string type
    // Check all expression parts for validity
    for (auto& part : node->parts) {
        if (part.type == frontend::TemplatePart::EXPR) {
            part.expr_value->accept(*this);
            // Could check if expression type is convertible to string
        }
    }
    current_expr_type = makeStringType();
}

// Visit TernaryExpr
void TypeChecker::visit(frontend::TernaryExpr* node) {
    // Check condition
    node->condition->accept(*this);
    auto cond_type = current_expr_type;
    
    // Condition should be boolean or integer (any integer can be used as boolean)
    if (cond_type->kind != TypeKind::BOOL && !cond_type->isNumeric()) {
        addError("Ternary condition must be boolean or numeric");
    }
    
    // Check true and false branches
    node->true_expr->accept(*this);
    auto true_type = current_expr_type;
    
    node->false_expr->accept(*this);
    auto false_type = current_expr_type;
    
    // Both branches should have compatible types
    // For now, simplified: use the true branch type
    current_expr_type = true_type;
}

// Visit BinaryOp
void TypeChecker::visit(frontend::BinaryOp* node) {
    // Get types of left and right operands
    node->left->accept(*this);
    auto left_type = current_expr_type;

    node->right->accept(*this);
    auto right_type = current_expr_type;

    // Check for errors
    if (left_type->kind == TypeKind::ERROR || right_type->kind == TypeKind::ERROR) {
        current_expr_type = makeErrorType();
        return;
    }

    // Type checking based on operator
    switch (node->op) {
        case frontend::BinaryOp::ADD:
        case frontend::BinaryOp::SUB:
        case frontend::BinaryOp::MUL:
        case frontend::BinaryOp::DIV:
        case frontend::BinaryOp::MOD:
            // Arithmetic operators require numeric types
            if (!left_type->isNumeric() || !right_type->isNumeric()) {
                addError("Arithmetic operators require numeric types");
                current_expr_type = makeErrorType();
                return;
            }
            // Result type is the wider of the two types (simplified)
            if (left_type->isFloat() || right_type->isFloat()) {
                current_expr_type = makeFloatType(64);
            } else {
                current_expr_type = makeIntType(64);
            }
            break;

        case frontend::BinaryOp::EQ:
        case frontend::BinaryOp::NE:
        case frontend::BinaryOp::LT:
        case frontend::BinaryOp::GT:
        case frontend::BinaryOp::LE:
        case frontend::BinaryOp::GE:
            // Comparison operators require compatible types, return bool
            if (!checkTypeCompatibility(*left_type, *right_type)) {
                addError("Incompatible types in comparison: " +
                        left_type->toString() + " and " + right_type->toString());
            }
            current_expr_type = makeBoolType();
            break;

        case frontend::BinaryOp::LOGICAL_AND:
        case frontend::BinaryOp::LOGICAL_OR:
            // Logical operators require bool types
            if (left_type->kind != TypeKind::BOOL || right_type->kind != TypeKind::BOOL) {
                addError("Logical operators require boolean operands");
            }
            current_expr_type = makeBoolType();
            break;

        case frontend::BinaryOp::BITWISE_AND:
        case frontend::BinaryOp::BITWISE_OR:
        case frontend::BinaryOp::BITWISE_XOR:
        case frontend::BinaryOp::LSHIFT:
        case frontend::BinaryOp::RSHIFT:
            // Bitwise operators require integer types
            if (!left_type->isInteger() || !right_type->isInteger()) {
                addError("Bitwise operators require integer types");
                current_expr_type = makeErrorType();
                return;
            }
            current_expr_type = makeIntType(64);
            break;
    }
}

// Visit UnaryOp
void TypeChecker::visit(frontend::UnaryOp* node) {
    node->operand->accept(*this);
    auto operand_type = current_expr_type;

    if (operand_type->kind == TypeKind::ERROR) {
        return;
    }

    switch (node->op) {
        case frontend::UnaryOp::NEG:
            if (!operand_type->isNumeric()) {
                addError("Unary minus requires numeric type");
                current_expr_type = makeErrorType();
                return;
            }
            // Result type is same as operand
            break;

        case frontend::UnaryOp::LOGICAL_NOT:
            if (operand_type->kind != TypeKind::BOOL) {
                addError("Logical NOT requires boolean type");
                current_expr_type = makeErrorType();
                return;
            }
            current_expr_type = makeBoolType();
            break;

        case frontend::UnaryOp::BITWISE_NOT:
            if (!operand_type->isInteger()) {
                addError("Bitwise NOT requires integer type");
                current_expr_type = makeErrorType();
                return;
            }
            break;

        case frontend::UnaryOp::POST_INC:
        case frontend::UnaryOp::POST_DEC:
            // Post-increment/decrement require numeric lvalue
            if (!operand_type->isNumeric()) {
                addError("Post-increment/decrement requires numeric type");
                current_expr_type = makeErrorType();
                return;
            }
            // Result type is same as operand
            // TODO: Verify operand is an lvalue (variable)
            break;

        case frontend::UnaryOp::ADDRESS_OF:
            // @ operator: takes any type and returns a pointer type
            // For now, we represent pointers as int64 (address)
            // In full implementation, would create a pointer type
            current_expr_type = makeIntType(64);
            break;

        case frontend::UnaryOp::PIN:
            // # operator: pins dynamic value to specific type
            // Takes dyn type and returns the pinned type
            // For now, return the operand type (simplified)
            // In full implementation, would track pinned type from context
            break;
    }
}

// Visit CallExpr
void TypeChecker::visit(frontend::CallExpr* node) {
    // For now, just assume function calls return int64
    // Full implementation would look up function signature
    current_expr_type = makeIntType(64);
}

// Visit LambdaExpr
void TypeChecker::visit(frontend::LambdaExpr* node) {
    // Lambda expressions evaluate to their return type
    current_expr_type = parseType(node->return_type);
    
    if (!current_expr_type) {
        addError("Unknown return type '" + node->return_type + "' in lambda expression");
        current_expr_type = makeErrorType();
        return;
    }
    
    // TODO: Type check lambda body in a new scope
    // TODO: If immediately invoked, check argument types match parameters
}

// Visit VarDecl
void TypeChecker::visit(frontend::VarDecl* node) {
    // Parse the declared type
    auto declared_type = parseType(node->type);
    
    if (!declared_type) {
        addError("Unknown type '" + node->type + "' in variable declaration for '" + node->name + "'");
        return;
    }

    // If there's an initializer, check its type
    if (node->initializer) {
        node->initializer->accept(*this);
        auto init_type = current_expr_type;
        
        if (!init_type) {
            addError("Could not determine type of initializer for '" + node->name + "'");
            return;
        }

        if (!checkTypeCompatibility(*declared_type, *init_type)) {
            addError("Type mismatch in variable declaration for '" + node->name +
                    "': expected " + declared_type->toString() +
                    ", got " + init_type->toString());
        }
    }

    // Add symbol to table
    if (!symbols->define(node->name, declared_type, false)) {
        addError("Redefinition of variable: " + node->name);
    }
}

// Visit ReturnStmt
void TypeChecker::visit(frontend::ReturnStmt* node) {
    if (node->value) {
        node->value->accept(*this);
        // Would check against function return type in full implementation
    }
}

// Visit IfStmt
void TypeChecker::visit(frontend::IfStmt* node) {
    // Check condition is boolean
    node->condition->accept(*this);
    if (current_expr_type->kind != TypeKind::BOOL) {
        addError("If condition must be boolean, got " + current_expr_type->toString());
    }

    // Check then branch
    if (node->then_block) {
        node->then_block->accept(*this);
    }

    // Check else branch if present
    if (node->else_block) {
        node->else_block->accept(*this);
    }
}

// Visit Block
void TypeChecker::visit(frontend::Block* node) {
    // Enter new scope
    auto old_symbols = std::move(symbols);
    symbols = std::make_unique<SymbolTable>(std::move(old_symbols));

    // Visit all statements
    for (auto& stmt : node->statements) {
        stmt->accept(*this);
    }

    // Exit scope (restore parent)
    // Note: In a real implementation, we'd properly manage scope chain
}

// Visit PickStmt
void TypeChecker::visit(frontend::PickStmt* node) {
    // Check selector expression
    if (node->selector) {
        node->selector->accept(*this);
    }

    // Check each case
    for (auto& case_node : node->cases) {
        if (case_node.value_start) {
            case_node.value_start->accept(*this);
        }
        if (case_node.body) {
            case_node.body->accept(*this);
        }
    }
}

// Visit TillLoop
void TypeChecker::visit(frontend::TillLoop* node) {
    // Check limit is integer
    if (node->limit) {
        node->limit->accept(*this);
        if (!current_expr_type->isInteger()) {
            addError("Till loop limit must be an integer");
        }
    }

    // Check step is integer
    if (node->step) {
        node->step->accept(*this);
        if (!current_expr_type->isInteger()) {
            addError("Till loop step must be an integer");
        }
    }

    if (node->body) {
        node->body->accept(*this);
    }
}

// Visit WhenLoop
void TypeChecker::visit(frontend::WhenLoop* node) {
    // When loops: when(condition) { body } then { success } end { failure }
    // Type check condition (should be boolean)
    if (node->condition) {
        node->condition->accept(*this);
    }
    // Type check all blocks
    if (node->body) {
        node->body->accept(*this);
    }
    if (node->then_block) {
        node->then_block->accept(*this);
    }
    if (node->end_block) {
        node->end_block->accept(*this);
    }
}

// Visit DeferStmt
void TypeChecker::visit(frontend::DeferStmt* node) {
    if (node->body) {
        node->body->accept(*this);
    }
}

// Helper: Get expression type
std::shared_ptr<Type> TypeChecker::getExpressionType(frontend::Expression* expr) {
    expr->accept(*this);
    return current_expr_type;
}

// Visit ForLoop
void TypeChecker::visit(frontend::ForLoop* node) {
    // Check iterable expression
    if (node->iterable) {
        node->iterable->accept(*this);
        // In full implementation, verify iterable type
    }
    
    // Check body with iterator variable in scope
    if (node->body) {
        node->body->accept(*this);
    }
}

// Visit WhileLoop
void TypeChecker::visit(frontend::WhileLoop* node) {
    // Check condition is boolean
    if (node->condition) {
        node->condition->accept(*this);
        if (current_expr_type->kind != TypeKind::BOOL) {
            addError("While condition must be boolean, got " + current_expr_type->toString());
        }
    }
    
    if (node->body) {
        node->body->accept(*this);
    }
}

// Visit BreakStmt
void TypeChecker::visit(frontend::BreakStmt* node) {
    // In full implementation, verify break is inside a loop
    // and label is valid if specified
}

// Visit ContinueStmt
void TypeChecker::visit(frontend::ContinueStmt* node) {
    // In full implementation, verify continue is inside a loop
    // and label is valid if specified
}

// Visit WhenExpr
void TypeChecker::visit(frontend::WhenExpr* node) {
    std::shared_ptr<Type> result_type = nullptr;
    
    // Check all cases
    for (auto& case_node : node->cases) {
        // Check condition is boolean
        if (case_node.condition) {
            case_node.condition->accept(*this);
            if (current_expr_type->kind != TypeKind::BOOL) {
                addError("When condition must be boolean");
            }
        }
        
        // Check result expression
        if (case_node.result) {
            case_node.result->accept(*this);
            if (!result_type) {
                result_type = current_expr_type;
            } else if (!result_type->equals(*current_expr_type)) {
                addError("All when branches must return same type");
            }
        }
    }
    
    // Check else result if present
    if (node->else_result) {
        node->else_result->accept(*this);
        if (result_type && !result_type->equals(*current_expr_type)) {
            addError("When else branch must match other branch types");
        }
    }
    
    current_expr_type = result_type ? result_type : makeIntType(64);
}

// Visit AwaitExpr
void TypeChecker::visit(frontend::AwaitExpr* node) {
    // Check the awaited expression
    if (node->expression) {
        node->expression->accept(*this);
        // In full implementation, verify expression is awaitable (async function call)
        // Result type is the inner type of the awaitable
    }
    // For now, preserve the expression's type
}

// Visit ObjectLiteral
void TypeChecker::visit(frontend::ObjectLiteral* node) {
    // Type check all field values
    for (auto& field : node->fields) {
        if (field.value) {
            field.value->accept(*this);
            // Could build struct type from field types
        }
    }
    // For now, object literals have dyn type (catch-all)
    // In full implementation, would create anonymous struct type
    current_expr_type = makeDynType();
}

// Visit MemberAccess
void TypeChecker::visit(frontend::MemberAccess* node) {
    // Type check the object being accessed
    if (node->object) {
        node->object->accept(*this);
        auto obj_type = current_expr_type;
        
        // In full implementation, would:
        // 1. Verify obj_type is a struct/object type
        // 2. Look up field in struct definition
        // 3. Return field's type
        
        // For now, assume member access returns int64 (simplified)
        current_expr_type = makeIntType(64);
    }
}

// Visit UnwrapExpr
void TypeChecker::visit(frontend::UnwrapExpr* node) {
    // Check the expression being unwrapped
    if (node->expression) {
        node->expression->accept(*this);
        auto expr_type = current_expr_type;
        
        // Unwrap operator (?) is used with Result types
        // In full implementation, would:
        // 1. Verify expr_type is a Result<T> type
        // 2. Return the inner type T
        
        // For now, preserve the expression type
    }
    
    // Check the default value if present
    if (node->default_value) {
        node->default_value->accept(*this);
        // In full implementation, verify default type matches unwrapped type
    }
}

// Helper: Check type compatibility
bool TypeChecker::checkTypeCompatibility(const Type& expected, const Type& actual) {
    // Exact match
    if (expected.equals(actual)) {
        return true;
    }

    // Allow numeric conversions (simplified)
    if (expected.isNumeric() && actual.isNumeric()) {
        return true;
    }

    return false;
}

// Main entry point
TypeCheckResult checkTypes(frontend::Block* ast) {
    TypeChecker checker;
    ast->accept(checker);
    return checker.getResult();
}

} // namespace sema
} // namespace aria
