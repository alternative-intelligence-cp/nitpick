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
#include <sstream>
#include <iostream>

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
    if (type_str == "func") return makeFuncType();
    if (type_str == "result" || type_str == "Result") return std::make_shared<Type>(TypeKind::STRUCT, "result");
    
    // SIMD Vector types
    if (type_str == "vec2") return std::make_shared<Type>(TypeKind::VEC2, "vec2");
    if (type_str == "vec3") return std::make_shared<Type>(TypeKind::VEC3, "vec3");
    if (type_str == "vec4") return std::make_shared<Type>(TypeKind::VEC4, "vec4");
    if (type_str == "dvec2") return std::make_shared<Type>(TypeKind::DVEC2, "dvec2");
    if (type_str == "dvec3") return std::make_shared<Type>(TypeKind::DVEC3, "dvec3");
    if (type_str == "dvec4") return std::make_shared<Type>(TypeKind::DVEC4, "dvec4");
    if (type_str == "ivec2") return std::make_shared<Type>(TypeKind::IVEC2, "ivec2");
    if (type_str == "ivec3") return std::make_shared<Type>(TypeKind::IVEC3, "ivec3");
    if (type_str == "ivec4") return std::make_shared<Type>(TypeKind::IVEC4, "ivec4");

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

// Visit FloatLiteral
void TypeChecker::visit(frontend::FloatLiteral* node) {
    // Float literals default to flt64 (double precision)
    current_expr_type = makeFloatType(64);
}

// Visit BoolLiteral
void TypeChecker::visit(frontend::BoolLiteral* node) {
    current_expr_type = makeBoolType();
}

// Visit NullLiteral
void TypeChecker::visit(frontend::NullLiteral* node) {
    // NULL is a generic pointer type (wild)
    current_expr_type = makeErrorType(); // TODO: Need pointer type system
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
    
    // Type check lambda body
    if (node->body) {
        node->body->accept(*this);
    }
    
    // TODO: Check function parameters
    // TODO: If immediately invoked, check argument types match parameters
}

// Visit VarDecl
void TypeChecker::visit(frontend::VarDecl* node) {
    // Skip type checking for generic function templates
    // They will be type-checked when monomorphized at call sites
    if (!node->generic_params.empty()) {
        // Generic function template - register in symbol table but don't type check
        // We'll type-check the instantiated versions later
        // For now, just mark it as a dynamic/template type
        auto template_type = std::make_shared<Type>(TypeKind::DYN, "template<" + node->name + ">");
        symbols->define(node->name, template_type, false);
        return;
    }
    
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
    // Check condition is boolean or numeric (numeric values are implicitly boolean)
    if (node->condition) {
        node->condition->accept(*this);
        if (current_expr_type->kind != TypeKind::BOOL && !current_expr_type->isNumeric()) {
            addError("While condition must be boolean or numeric, got " + current_expr_type->toString());
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
// Visit VectorLiteral - Validate vector/matrix constructors
void TypeChecker::visit(frontend::VectorLiteral* node) {
    // Map vector type name to expected component count and base type
    struct VectorTypeInfo {
        int component_count;
        TypeKind element_kind;
        std::string element_type_name;
    };
    
    std::map<std::string, VectorTypeInfo> vector_types = {
        {"vec2",  {2, TypeKind::FLT32, "flt32"}},
        {"vec3",  {3, TypeKind::FLT32, "flt32"}},
        {"vec4",  {4, TypeKind::FLT32, "flt32"}},
        {"vec9",  {9, TypeKind::FLT32, "flt32"}},
        {"dvec2", {2, TypeKind::FLT64, "flt64"}},
        {"dvec3", {3, TypeKind::FLT64, "flt64"}},
        {"dvec4", {4, TypeKind::FLT64, "flt64"}},
        {"ivec2", {2, TypeKind::INT32, "int32"}},
        {"ivec3", {3, TypeKind::INT32, "int32"}},
        {"ivec4", {4, TypeKind::INT32, "int32"}},
        {"uvec2", {2, TypeKind::UINT32, "uint32"}},
        {"uvec3", {3, TypeKind::UINT32, "uint32"}},
        {"uvec4", {4, TypeKind::UINT32, "uint32"}},
        {"bvec2", {2, TypeKind::BOOL, "bool"}},
        {"bvec3", {3, TypeKind::BOOL, "bool"}},
        {"bvec4", {4, TypeKind::BOOL, "bool"}},
        // Matrices (for now, just validate component counts)
        {"mat2",  {4, TypeKind::FLT32, "flt32"}},   // 2x2 = 4
        {"mat3",  {9, TypeKind::FLT32, "flt32"}},   // 3x3 = 9
        {"mat4",  {16, TypeKind::FLT32, "flt32"}},  // 4x4 = 16
        {"mat2x3", {6, TypeKind::FLT32, "flt32"}},  // 2 cols * 3 rows
        {"mat2x4", {8, TypeKind::FLT32, "flt32"}},
        {"mat3x2", {6, TypeKind::FLT32, "flt32"}},
        {"mat3x4", {12, TypeKind::FLT32, "flt32"}},
        {"mat4x2", {8, TypeKind::FLT32, "flt32"}},
        {"mat4x3", {12, TypeKind::FLT32, "flt32"}},
        {"dmat2",  {4, TypeKind::FLT64, "flt64"}},
        {"dmat3",  {9, TypeKind::FLT64, "flt64"}},
        {"dmat4",  {16, TypeKind::FLT64, "flt64"}},
        {"dmat2x3", {6, TypeKind::FLT64, "flt64"}},
        {"dmat2x4", {8, TypeKind::FLT64, "flt64"}},
        {"dmat3x2", {6, TypeKind::FLT64, "flt64"}},
        {"dmat3x4", {12, TypeKind::FLT64, "flt64"}},
        {"dmat4x2", {8, TypeKind::FLT64, "flt64"}},
        {"dmat4x3", {12, TypeKind::FLT64, "flt64"}},
    };
    
    // Lookup the vector type info
    auto it = vector_types.find(node->vector_type);
    if (it == vector_types.end()) {
        std::ostringstream oss;
        oss << "Unknown vector/matrix type: " << node->vector_type;
        addError(oss.str());
        current_expr_type = makeErrorType();
        return;
    }
    
    const VectorTypeInfo& info = it->second;
    int expected_components = info.component_count;
    TypeKind expected_element_kind = info.element_kind;
    
    // CASE 1: Empty constructor - allowed, will zero-initialize
    if (node->elements.empty()) {
        // Valid: vec4() creates {0, 0, 0, 0}
        current_expr_type = parseType(node->vector_type);
        return;
    }
    
    // CASE 2: Single scalar argument - broadcasting (splat)
    if (node->elements.size() == 1) {
        node->elements[0]->accept(*this);
        auto arg_type = current_expr_type;
        
        // Check if it's a scalar, not another vector
        if (arg_type->kind >= TypeKind::VEC2 && arg_type->kind <= TypeKind::IVEC4) {
            std::ostringstream oss;
            oss << "Cannot broadcast vector to " << node->vector_type 
                << " - single argument must be scalar for broadcasting";
            addError(oss.str());
            current_expr_type = makeErrorType();
            return;
        }
        
        // Check type compatibility
        bool compatible = false;
        if (expected_element_kind == TypeKind::BOOL) {
            compatible = (arg_type->kind == TypeKind::BOOL);
        } else if (expected_element_kind == TypeKind::INT32 || expected_element_kind == TypeKind::UINT32) {
            compatible = arg_type->isInteger();
        } else if (expected_element_kind == TypeKind::FLT32 || expected_element_kind == TypeKind::FLT64) {
            compatible = arg_type->isNumeric();  // Allow int->float conversion
        }
        
        if (!compatible) {
            std::ostringstream oss;
            oss << "Type mismatch in " << node->vector_type << " constructor: expected "
                << info.element_type_name << ", got " << arg_type->toString();
            addError(oss.str());
        }
        
        // Valid broadcasting: vec4(1.0) -> {1.0, 1.0, 1.0, 1.0}
        current_expr_type = parseType(node->vector_type);
        return;
    }
    
    // CASE 3: Multiple arguments - component-wise or composition construction
    // Flatten arguments and count total components
    int total_components = 0;
    std::vector<std::shared_ptr<Type>> element_types;
    
    for (auto& elem : node->elements) {
        elem->accept(*this);
        auto elem_type = current_expr_type;
        element_types.push_back(elem_type);
        
        // Check if element is a vector itself (composition)
        int elem_components = 1;  // Default: scalar = 1 component
        if (elem_type->kind >= TypeKind::VEC2 && elem_type->kind <= TypeKind::IVEC4) {
            // It's a vector - count its components
            switch (elem_type->kind) {
                case TypeKind::VEC2:
                case TypeKind::DVEC2:
                case TypeKind::IVEC2:
                    elem_components = 2; break;
                case TypeKind::VEC3:
                case TypeKind::DVEC3:
                case TypeKind::IVEC3:
                    elem_components = 3; break;
                case TypeKind::VEC4:
                case TypeKind::DVEC4:
                case TypeKind::IVEC4:
                    elem_components = 4; break;
                default:
                    elem_components = 1;
            }
        }
        
        total_components += elem_components;
    }
    
    // Validate component count
    if (total_components != expected_components) {
        std::ostringstream oss;
        oss << node->vector_type << " constructor requires " << expected_components
            << " components, but " << total_components << " were provided";
        addError(oss.str());
        current_expr_type = makeErrorType();
        return;
    }
    
    // Validate element types
    for (auto& elem_type : element_types) {
        bool compatible = false;
        
        // If element is a vector, check base compatibility
        if (elem_type->kind >= TypeKind::VEC2 && elem_type->kind <= TypeKind::IVEC4) {
            // Composition case: vec4(vec2(...), z, w)
            // Check that the vector's base type is compatible
            if (expected_element_kind == TypeKind::FLT32 || expected_element_kind == TypeKind::FLT64) {
                // Float vectors accept float vectors
                compatible = (elem_type->kind >= TypeKind::VEC2 && elem_type->kind <= TypeKind::DVEC4);
            } else if (expected_element_kind == TypeKind::INT32 || expected_element_kind == TypeKind::UINT32) {
                // Integer vectors accept integer vectors
                compatible = (elem_type->kind >= TypeKind::IVEC2 && elem_type->kind <= TypeKind::IVEC4);
            }
        } else {
            // Scalar case
            if (expected_element_kind == TypeKind::BOOL) {
                compatible = (elem_type->kind == TypeKind::BOOL);
            } else if (expected_element_kind == TypeKind::INT32 || expected_element_kind == TypeKind::UINT32) {
                compatible = elem_type->isInteger();
            } else if (expected_element_kind == TypeKind::FLT32 || expected_element_kind == TypeKind::FLT64) {
                compatible = elem_type->isNumeric();  // Allow int->float implicit conversion
            }
        }
        
        if (!compatible) {
            std::ostringstream oss;
            oss << "Type mismatch in " << node->vector_type << " constructor: expected "
                << info.element_type_name << " components, got " << elem_type->toString();
            addError(oss.str());
        }
    }
    
    // Set result type
    current_expr_type = parseType(node->vector_type);
}

bool TypeChecker::checkTypeCompatibility(const Type& expected, const Type& actual) {
    // Exact match
    if (expected.equals(actual)) {
        return true;
    }

    // Allow numeric conversions (simplified)
    if (expected.isNumeric() && actual.isNumeric()) {
        return true;
    }

    // func type can accept any function type (including lambdas with any return type)
    // This allows: func:greet = void(){...} or func:add = int8(){...}
    if (expected.kind == TypeKind::FUNCTION) {
        // Accept any type for func (lambdas evaluate to their return type)
        // In full implementation, would check function signature
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
