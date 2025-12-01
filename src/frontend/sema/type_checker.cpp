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
    }
}

// Visit CallExpr
void TypeChecker::visit(frontend::CallExpr* node) {
    // For now, just assume function calls return int64
    // Full implementation would look up function signature
    current_expr_type = makeIntType(64);
}

// Visit VarDecl
void TypeChecker::visit(frontend::VarDecl* node) {
    // Parse the declared type
    auto declared_type = parseType(node->type);

    // If there's an initializer, check its type
    if (node->initializer) {
        node->initializer->accept(*this);
        auto init_type = current_expr_type;

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
