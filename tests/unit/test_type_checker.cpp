#include "test_helpers.h"
#include "frontend/sema/type_checker.h"
#include "frontend/sema/symbol_table.h"
#include "frontend/sema/type.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include "frontend/token.h"

using namespace aria;
using namespace aria::sema;
using namespace aria::frontend;

// ============================================================================
// Type Checker Tests - Phase 3.2.2
// ============================================================================

// ----------------------------------------------------------------------------
// Literal Type Inference Tests
// ----------------------------------------------------------------------------

TEST_CASE(type_checker_literal_int) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    LiteralExpr expr(int64_t(42));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "int64", "Integer literal should be int64");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_literal_float) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    LiteralExpr expr(3.14);
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "flt64", "Float literal should be flt64");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_literal_string) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    LiteralExpr expr(std::string("hello"));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "string", "String literal should be string");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_literal_bool) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    LiteralExpr expr(true);
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "bool", "Boolean literal should be bool");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_literal_null) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    LiteralExpr expr(std::monostate{});
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT(type->getKind() == TypeKind::UNKNOWN, "Null literal should be UnknownType");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

// ----------------------------------------------------------------------------
// Identifier Type Inference Tests
// ----------------------------------------------------------------------------

TEST_CASE(type_checker_identifier_defined) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // Define variable
    auto intType = types.getPrimitiveType("int32");
    symbols.defineSymbol("x", SymbolKind::VARIABLE, intType);
    
    // Infer identifier type
    IdentifierExpr expr("x");
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "int32", "Identifier should have defined type");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_identifier_undefined) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    IdentifierExpr expr("nonexistent");
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT(type->getKind() == TypeKind::ERROR, "Undefined identifier should be ErrorType");
    ASSERT(checker.hasErrors(), "Should have error");
}

// ----------------------------------------------------------------------------
// Binary Operator Type Inference Tests - Arithmetic
// ----------------------------------------------------------------------------

TEST_CASE(type_checker_binary_add_int) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto left = std::make_unique<LiteralExpr>(int64_t(10));
    auto right = std::make_unique<LiteralExpr>(int64_t(20));
    Token op(TokenType::TOKEN_PLUS, "+", 1, 1);
    
    BinaryExpr expr(std::move(left), op, std::move(right));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "int64", "Addition should produce int64");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_binary_subtract_float) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto left = std::make_unique<LiteralExpr>(3.14);
    auto right = std::make_unique<LiteralExpr>(1.0);
    Token op(TokenType::TOKEN_MINUS, "-", 1, 1);
    
    BinaryExpr expr(std::move(left), op, std::move(right));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "flt64", "Subtraction should produce flt64");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_binary_multiply) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto left = std::make_unique<LiteralExpr>(int64_t(5));
    auto right = std::make_unique<LiteralExpr>(int64_t(3));
    Token op(TokenType::TOKEN_STAR, "*", 1, 1);
    
    BinaryExpr expr(std::move(left), op, std::move(right));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "int64", "Multiplication should produce int64");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_binary_divide) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto left = std::make_unique<LiteralExpr>(int64_t(10));
    auto right = std::make_unique<LiteralExpr>(int64_t(2));
    Token op(TokenType::TOKEN_SLASH, "/", 1, 1);
    
    BinaryExpr expr(std::move(left), op, std::move(right));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "int64", "Division should produce int64");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_binary_modulo) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto left = std::make_unique<LiteralExpr>(int64_t(10));
    auto right = std::make_unique<LiteralExpr>(int64_t(3));
    Token op(TokenType::TOKEN_PERCENT, "%", 1, 1);
    
    BinaryExpr expr(std::move(left), op, std::move(right));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "int64", "Modulo should produce int64");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

// ----------------------------------------------------------------------------
// Binary Operator Type Inference Tests - Comparison
// ----------------------------------------------------------------------------

TEST_CASE(type_checker_binary_equal) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto left = std::make_unique<LiteralExpr>(int64_t(10));
    auto right = std::make_unique<LiteralExpr>(int64_t(10));
    Token op(TokenType::TOKEN_EQUAL_EQUAL, "==", 1, 1);
    
    BinaryExpr expr(std::move(left), op, std::move(right));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "bool", "Equality should produce bool");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_binary_not_equal) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto left = std::make_unique<LiteralExpr>(int64_t(10));
    auto right = std::make_unique<LiteralExpr>(int64_t(20));
    Token op(TokenType::TOKEN_BANG_EQUAL, "!=", 1, 1);
    
    BinaryExpr expr(std::move(left), op, std::move(right));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "bool", "Not-equal should produce bool");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_binary_less_than) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto left = std::make_unique<LiteralExpr>(int64_t(5));
    auto right = std::make_unique<LiteralExpr>(int64_t(10));
    Token op(TokenType::TOKEN_LESS, "<", 1, 1);
    
    BinaryExpr expr(std::move(left), op, std::move(right));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "bool", "Less-than should produce bool");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_binary_greater_than) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto left = std::make_unique<LiteralExpr>(int64_t(15));
    auto right = std::make_unique<LiteralExpr>(int64_t(10));
    Token op(TokenType::TOKEN_GREATER, ">", 1, 1);
    
    BinaryExpr expr(std::move(left), op, std::move(right));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "bool", "Greater-than should produce bool");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

// ----------------------------------------------------------------------------
// Binary Operator Type Inference Tests - Logical
// ----------------------------------------------------------------------------

TEST_CASE(type_checker_binary_logical_and) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto left = std::make_unique<LiteralExpr>(true);
    auto right = std::make_unique<LiteralExpr>(false);
    Token op(TokenType::TOKEN_AND_AND, "&&", 1, 1);
    
    BinaryExpr expr(std::move(left), op, std::move(right));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "bool", "Logical AND should produce bool");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_binary_logical_or) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto left = std::make_unique<LiteralExpr>(true);
    auto right = std::make_unique<LiteralExpr>(false);
    Token op(TokenType::TOKEN_OR_OR, "||", 1, 1);
    
    BinaryExpr expr(std::move(left), op, std::move(right));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "bool", "Logical OR should produce bool");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

// ----------------------------------------------------------------------------
// Binary Operator Type Checking - Error Cases
// ----------------------------------------------------------------------------

TEST_CASE(type_checker_binary_logical_and_non_bool) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto left = std::make_unique<LiteralExpr>(int64_t(1));  // Not bool!
    auto right = std::make_unique<LiteralExpr>(true);
    Token op(TokenType::TOKEN_AND_AND, "&&", 1, 1);
    
    BinaryExpr expr(std::move(left), op, std::move(right));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT(type->getKind() == TypeKind::ERROR, "Should produce ErrorType");
    ASSERT(checker.hasErrors(), "Should have error (no truthiness)");
}

TEST_CASE(type_checker_binary_add_incompatible) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto left = std::make_unique<LiteralExpr>(int64_t(10));
    auto right = std::make_unique<LiteralExpr>(std::string("hello"));
    Token op(TokenType::TOKEN_PLUS, "+", 1, 1);
    
    BinaryExpr expr(std::move(left), op, std::move(right));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT(type->getKind() == TypeKind::ERROR, "Should produce ErrorType");
    ASSERT(checker.hasErrors(), "Should have error");
}

// ----------------------------------------------------------------------------
// Unary Operator Type Inference Tests
// ----------------------------------------------------------------------------

TEST_CASE(type_checker_unary_negate) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto operand = std::make_unique<LiteralExpr>(int64_t(42));
    Token op(TokenType::TOKEN_MINUS, "-", 1, 1);
    
    UnaryExpr expr(op, std::move(operand));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "int64", "Negation should produce int64");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_unary_logical_not) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto operand = std::make_unique<LiteralExpr>(true);
    Token op(TokenType::TOKEN_BANG, "!", 1, 1);
    
    UnaryExpr expr(op, std::move(operand));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "bool", "Logical NOT should produce bool");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_unary_logical_not_non_bool) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto operand = std::make_unique<LiteralExpr>(int64_t(1));  // Not bool!
    Token op(TokenType::TOKEN_BANG, "!", 1, 1);
    
    UnaryExpr expr(op, std::move(operand));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT(type->getKind() == TypeKind::ERROR, "Should produce ErrorType");
    ASSERT(checker.hasErrors(), "Should have error (no truthiness)");
}

// ----------------------------------------------------------------------------
// Ternary Operator Type Inference Tests
// ----------------------------------------------------------------------------

TEST_CASE(type_checker_ternary_basic) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto cond = std::make_unique<LiteralExpr>(true);
    auto trueVal = std::make_unique<LiteralExpr>(int64_t(10));
    auto falseVal = std::make_unique<LiteralExpr>(int64_t(20));
    
    TernaryExpr expr(std::move(cond), std::move(trueVal), std::move(falseVal));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "int64", "Ternary should produce common type");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_ternary_condition_must_be_bool) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto cond = std::make_unique<LiteralExpr>(int64_t(1));  // Not bool!
    auto trueVal = std::make_unique<LiteralExpr>(int64_t(10));
    auto falseVal = std::make_unique<LiteralExpr>(int64_t(20));
    
    TernaryExpr expr(std::move(cond), std::move(trueVal), std::move(falseVal));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT(type->getKind() == TypeKind::ERROR, "Should produce ErrorType");
    ASSERT(checker.hasErrors(), "Should have error (condition must be bool)");
}

TEST_CASE(type_checker_ternary_incompatible_branches) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    auto cond = std::make_unique<LiteralExpr>(true);
    auto trueVal = std::make_unique<LiteralExpr>(int64_t(10));
    auto falseVal = std::make_unique<LiteralExpr>(std::string("hello"));  // Incompatible!
    
    TernaryExpr expr(std::move(cond), std::move(trueVal), std::move(falseVal));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT(type->getKind() == TypeKind::ERROR, "Should produce ErrorType");
    ASSERT(checker.hasErrors(), "Should have error (incompatible branch types)");
}

// ----------------------------------------------------------------------------
// Type Coercion Tests
// ----------------------------------------------------------------------------

TEST_CASE(type_checker_coercion_int_widening) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // int8 can coerce to int16
    auto int8Type = types.getPrimitiveType("int8");
    auto int16Type = types.getPrimitiveType("int16");
    
    // Use reflection to test canCoerce (assuming it's public or we have access)
    // For now, test indirectly through findCommonType
    symbols.defineSymbol("x", SymbolKind::VARIABLE, int8Type);
    symbols.defineSymbol("y", SymbolKind::VARIABLE, int16Type);
    
    // Create expression: x + y (should widen to int16)
    auto left = std::make_unique<IdentifierExpr>("x");
    auto right = std::make_unique<IdentifierExpr>("y");
    Token op(TokenType::TOKEN_PLUS, "+", 1, 1);
    
    BinaryExpr expr(std::move(left), op, std::move(right));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "int16", "Should widen to int16");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

// ----------------------------------------------------------------------------
// Complex Expression Tests
// ----------------------------------------------------------------------------

TEST_CASE(type_checker_complex_arithmetic) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // (5 + 3) * 2
    auto five = std::make_unique<LiteralExpr>(int64_t(5));
    auto three = std::make_unique<LiteralExpr>(int64_t(3));
    Token plusOp(TokenType::TOKEN_PLUS, "+", 1, 1);
    auto add = std::make_unique<BinaryExpr>(std::move(five), plusOp, std::move(three));
    
    auto two = std::make_unique<LiteralExpr>(int64_t(2));
    Token mulOp(TokenType::TOKEN_STAR, "*", 1, 1);
    
    BinaryExpr expr(std::move(add), mulOp, std::move(two));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "int64", "Complex expression should produce int64");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_complex_comparison) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // (x > 5) && (y < 10)
    auto intType = types.getPrimitiveType("int32");
    symbols.defineSymbol("x", SymbolKind::VARIABLE, intType);
    symbols.defineSymbol("y", SymbolKind::VARIABLE, intType);
    
    auto x1 = std::make_unique<IdentifierExpr>("x");
    auto five = std::make_unique<LiteralExpr>(int64_t(5));
    Token gtOp(TokenType::TOKEN_GREATER, ">", 1, 1);
    auto comp1 = std::make_unique<BinaryExpr>(std::move(x1), gtOp, std::move(five));
    
    auto y = std::make_unique<IdentifierExpr>("y");
    auto ten = std::make_unique<LiteralExpr>(int64_t(10));
    Token ltOp(TokenType::TOKEN_LESS, "<", 1, 1);
    auto comp2 = std::make_unique<BinaryExpr>(std::move(y), ltOp, std::move(ten));
    
    Token andOp(TokenType::TOKEN_AND_AND, "&&", 1, 1);
    BinaryExpr expr(std::move(comp1), andOp, std::move(comp2));
    Type* type = checker.inferType(&expr);
    
    ASSERT(type != nullptr, "Type should be inferred");
    ASSERT_EQ(type->toString(), "bool", "Complex comparison should produce bool");
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

// ----------------------------------------------------------------------------
// Statement Type Checking Tests - Phase 3.2.3
// ----------------------------------------------------------------------------

TEST_CASE(type_checker_var_decl_with_initializer) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // int64:x = 42;
    auto init = std::make_unique<LiteralExpr>(int64_t(42));
    aria::VarDeclStmt stmt("int64", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    if (checker.hasErrors()) {
        for (const auto& err : checker.getErrors()) {
            std::cout << "Error: " << err << std::endl;
        }
    }
    ASSERT(!checker.hasErrors(), "Should have no errors");
    
    // Verify symbol was defined
    auto symbol = symbols.lookupSymbol("x");
    ASSERT(symbol != nullptr, "Symbol should be defined");
    if (symbol) {
        ASSERT_EQ(symbol->type->toString(), "int64", "Symbol should have correct type");
    }
}

TEST_CASE(type_checker_var_decl_without_initializer) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // int64:x;
    aria::VarDeclStmt stmt("int64", "x", nullptr);
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors");
    
    // Verify symbol was defined
    auto symbol = symbols.lookupSymbol("x");
    ASSERT(symbol != nullptr, "Symbol should be defined");
}

TEST_CASE(type_checker_var_decl_type_mismatch) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // int64:x = "hello";  // Type mismatch!
    auto init = std::make_unique<LiteralExpr>(std::string("hello"));
    aria::VarDeclStmt stmt("int64", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error for type mismatch");
}

TEST_CASE(type_checker_const_var_without_initializer) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // const int64:x;  // Error: const must have initializer
    aria::VarDeclStmt stmt("int64", "x", nullptr);
    stmt.isConst = true;
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error (const needs initializer)");
}

TEST_CASE(type_checker_assignment_compatible_types) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // Define variable
    auto intType = types.getPrimitiveType("int64");
    symbols.defineSymbol("x", SymbolKind::VARIABLE, intType);
    
    // x = 42
    auto left = std::make_unique<IdentifierExpr>("x");
    auto right = std::make_unique<LiteralExpr>(int64_t(42));
    Token op(TokenType::TOKEN_EQUAL, "=", 1, 1);
    BinaryExpr expr(std::move(left), op, std::move(right));
    
    checker.checkAssignment(&expr);
    
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_assignment_incompatible_types) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // Define variable
    auto intType = types.getPrimitiveType("int64");
    symbols.defineSymbol("x", SymbolKind::VARIABLE, intType);
    
    // x = "hello"  // Type mismatch!
    auto left = std::make_unique<IdentifierExpr>("x");
    auto right = std::make_unique<LiteralExpr>(std::string("hello"));
    Token op(TokenType::TOKEN_EQUAL, "=", 1, 1);
    BinaryExpr expr(std::move(left), op, std::move(right));
    
    checker.checkAssignment(&expr);
    
    ASSERT(checker.hasErrors(), "Should have error for type mismatch");
}

TEST_CASE(type_checker_assignment_to_const) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // Define const variable
    auto intType = types.getPrimitiveType("int64");
    symbols.defineSymbol("x", SymbolKind::CONSTANT, intType);
    
    // x = 42  // Error: cannot assign to const!
    auto left = std::make_unique<IdentifierExpr>("x");
    auto right = std::make_unique<LiteralExpr>(int64_t(42));
    Token op(TokenType::TOKEN_EQUAL, "=", 1, 1);
    BinaryExpr expr(std::move(left), op, std::move(right));
    
    checker.checkAssignment(&expr);
    
    ASSERT(checker.hasErrors(), "Should have error (cannot assign to const)");
}

TEST_CASE(type_checker_return_void_function) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // Set function return type to void
    auto voidType = types.getPrimitiveType("void");
    checker.setCurrentFunctionReturnType(voidType);
    
    // return;
    aria::ReturnStmt stmt(nullptr);
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_return_void_with_value) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // Set function return type to void
    auto voidType = types.getPrimitiveType("void");
    checker.setCurrentFunctionReturnType(voidType);
    
    // return 42;  // Error: void function cannot return value!
    auto value = std::make_unique<LiteralExpr>(int64_t(42));
    aria::ReturnStmt stmt(std::move(value));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error (void function cannot return value)");
}

TEST_CASE(type_checker_return_non_void_without_value) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // Set function return type to int32
    auto intType = types.getPrimitiveType("int32");
    checker.setCurrentFunctionReturnType(intType);
    
    // return;  // Error: non-void function must return value!
    aria::ReturnStmt stmt(nullptr);
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error (non-void function must return value)");
}

TEST_CASE(type_checker_return_correct_type) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // Set function return type to int64
    auto intType = types.getPrimitiveType("int64");
    checker.setCurrentFunctionReturnType(intType);
    
    // return 42;
    auto value = std::make_unique<LiteralExpr>(int64_t(42));
    aria::ReturnStmt stmt(std::move(value));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_return_wrong_type) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // Set function return type to int64
    auto intType = types.getPrimitiveType("int64");
    checker.setCurrentFunctionReturnType(intType);
    
    // return "hello";  // Type mismatch!
    auto value = std::make_unique<LiteralExpr>(std::string("hello"));
    aria::ReturnStmt stmt(std::move(value));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error (return type mismatch)");
}

TEST_CASE(type_checker_if_condition_bool) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // if (true) { }
    auto cond = std::make_unique<LiteralExpr>(true);
    auto thenBlock = std::make_unique<aria::BlockStmt>();
    
    aria::IfStmt stmt(std::move(cond), std::move(thenBlock));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_if_condition_non_bool) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // if (42) { }  // Error: condition must be bool!
    auto cond = std::make_unique<LiteralExpr>(int64_t(42));
    auto thenBlock = std::make_unique<aria::BlockStmt>();
    
    aria::IfStmt stmt(std::move(cond), std::move(thenBlock));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error (condition must be bool)");
}

TEST_CASE(type_checker_while_condition_bool) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // while (true) { }
    auto cond = std::make_unique<LiteralExpr>(true);
    auto body = std::make_unique<aria::BlockStmt>();
    
    aria::WhileStmt stmt(std::move(cond), std::move(body));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_while_condition_non_bool) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // while (42) { }  // Error: condition must be bool!
    auto cond = std::make_unique<LiteralExpr>(int64_t(42));
    auto body = std::make_unique<aria::BlockStmt>();
    
    aria::WhileStmt stmt(std::move(cond), std::move(body));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error (condition must be bool)");
}

TEST_CASE(type_checker_for_condition_bool) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // for (int64:i = 0; i < 10; i = i + 1) { }
    auto intType = types.getPrimitiveType("int64");
    
    // Initializer: int64:i = 0
    auto init_val = std::make_unique<LiteralExpr>(int64_t(0));
    auto init = std::make_unique<aria::VarDeclStmt>("int64", "i", std::move(init_val));
    
    // Condition: i < 10
    symbols.defineSymbol("i", SymbolKind::VARIABLE, intType);  // Pre-define for condition checking
    auto i1 = std::make_unique<IdentifierExpr>("i");
    auto ten = std::make_unique<LiteralExpr>(int64_t(10));
    Token ltOp(TokenType::TOKEN_LESS, "<", 1, 1);
    auto cond = std::make_unique<BinaryExpr>(std::move(i1), ltOp, std::move(ten));
    
    // Update: i = i + 1
    auto i2 = std::make_unique<IdentifierExpr>("i");
    auto i3 = std::make_unique<IdentifierExpr>("i");
    auto one = std::make_unique<LiteralExpr>(int64_t(1));
    Token plusOp(TokenType::TOKEN_PLUS, "+", 1, 1);
    auto addExpr = std::make_unique<BinaryExpr>(std::move(i3), plusOp, std::move(one));
    Token eqOp(TokenType::TOKEN_EQUAL, "=", 1, 1);
    auto update = std::make_unique<BinaryExpr>(std::move(i2), eqOp, std::move(addExpr));
    
    auto body = std::make_unique<aria::BlockStmt>();
    
    aria::ForStmt stmt(std::move(init), std::move(cond), std::move(update), std::move(body));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

TEST_CASE(type_checker_block_with_scoping) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // {
    //   int64:x = 10;
    //   int64:y = 20;
    // }
    auto x_init = std::make_unique<LiteralExpr>(int64_t(10));
    auto x_decl = std::make_unique<aria::VarDeclStmt>("int64", "x", std::move(x_init));
    
    auto y_init = std::make_unique<LiteralExpr>(int64_t(20));
    auto y_decl = std::make_unique<aria::VarDeclStmt>("int64", "y", std::move(y_init));
    
    std::vector<ASTNodePtr> stmts;
    stmts.push_back(std::move(x_decl));
    stmts.push_back(std::move(y_decl));
    
    aria::BlockStmt block(std::move(stmts));
    
    checker.checkStatement(&block);
    
    ASSERT(!checker.hasErrors(), "Should have no errors");
}

// ----------------------------------------------------------------------------
// TBB Type Validation Tests - Phase 3.2.4
// ----------------------------------------------------------------------------

TEST_CASE(type_checker_tbb8_valid_value) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // tbb8:x = 100;  // Valid: within [-127, +127]
    auto init = std::make_unique<LiteralExpr>(int64_t(100));
    aria::VarDeclStmt stmt("tbb8", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors for valid tbb8 value");
}

TEST_CASE(type_checker_tbb8_err_sentinel_warning) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // tbb8:x = -128;  // Warning: ERR sentinel value
    auto init = std::make_unique<LiteralExpr>(int64_t(-128));
    aria::VarDeclStmt stmt("tbb8", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have warning for ERR sentinel");
}

TEST_CASE(type_checker_tbb8_out_of_range_positive) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // tbb8:x = 200;  // Error: out of range (max is +127)
    auto init = std::make_unique<LiteralExpr>(int64_t(200));
    aria::VarDeclStmt stmt("tbb8", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error for out of range value");
}

TEST_CASE(type_checker_tbb8_out_of_range_negative) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // tbb8:x = -200;  // Error: out of range (min is -127, -128 is ERR)
    auto init = std::make_unique<LiteralExpr>(int64_t(-200));
    aria::VarDeclStmt stmt("tbb8", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error for out of range value");
}

TEST_CASE(type_checker_tbb16_valid_value) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // tbb16:x = 10000;  // Valid: within [-32767, +32767]
    auto init = std::make_unique<LiteralExpr>(int64_t(10000));
    aria::VarDeclStmt stmt("tbb16", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors for valid tbb16 value");
}

TEST_CASE(type_checker_tbb16_err_sentinel_warning) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // tbb16:x = -32768;  // Warning: ERR sentinel value
    auto init = std::make_unique<LiteralExpr>(int64_t(-32768));
    aria::VarDeclStmt stmt("tbb16", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have warning for ERR sentinel");
}

TEST_CASE(type_checker_tbb16_out_of_range) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // tbb16:x = 40000;  // Error: out of range (max is +32767)
    auto init = std::make_unique<LiteralExpr>(int64_t(40000));
    aria::VarDeclStmt stmt("tbb16", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error for out of range value");
}

TEST_CASE(type_checker_tbb_no_coercion_from_int) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // int64:y = 10; tbb8:x = y;  // Error: cannot coerce int64 to tbb8
    auto intType = types.getPrimitiveType("int64");
    symbols.defineSymbol("y", SymbolKind::VARIABLE, intType);
    
    auto init = std::make_unique<IdentifierExpr>("y");
    aria::VarDeclStmt stmt("tbb8", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error (int64 → tbb8 not allowed)");
}

TEST_CASE(type_checker_tbb_no_coercion_to_int) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // tbb8:y = 10; int64:x = y;  // Error: cannot coerce tbb8 to int64
    auto tbbType = types.getPrimitiveType("tbb8");
    symbols.defineSymbol("y", SymbolKind::VARIABLE, tbbType);
    
    auto init = std::make_unique<IdentifierExpr>("y");
    aria::VarDeclStmt stmt("int64", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error (tbb8 → int64 not allowed)");
}

TEST_CASE(type_checker_tbb_widening_allowed) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // tbb8:y = 10; tbb16:x = y;  // OK: tbb8 → tbb16 widening allowed
    auto tbb8Type = types.getPrimitiveType("tbb8");
    symbols.defineSymbol("y", SymbolKind::VARIABLE, tbb8Type);
    
    auto init = std::make_unique<IdentifierExpr>("y");
    aria::VarDeclStmt stmt("tbb16", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors (tbb8 → tbb16 widening)");
}

TEST_CASE(type_checker_tbb_assignment_err_sentinel) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // tbb8:x; x = -128;  // Warning: assigning ERR sentinel
    auto tbbType = types.getPrimitiveType("tbb8");
    symbols.defineSymbol("x", SymbolKind::VARIABLE, tbbType);
    
    auto left = std::make_unique<IdentifierExpr>("x");
    auto right = std::make_unique<LiteralExpr>(int64_t(-128));
    Token op(TokenType::TOKEN_EQUAL, "=", 1, 1);
    BinaryExpr expr(std::move(left), op, std::move(right));
    
    checker.checkAssignment(&expr);
    
    ASSERT(checker.hasErrors(), "Should have warning for ERR sentinel assignment");
}

TEST_CASE(type_checker_tbb32_valid_value) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // tbb32:x = 1000000;  // Valid
    auto init = std::make_unique<LiteralExpr>(int64_t(1000000));
    aria::VarDeclStmt stmt("tbb32", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors for valid tbb32 value");
}

// ----------------------------------------------------------------------------
// Balanced Ternary/Nonary Type Validation Tests - Phase 3.2.5
// ----------------------------------------------------------------------------

TEST_CASE(type_checker_trit_valid_negative) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // trit:x = -1;  // Valid: trit must be -1, 0, or 1
    auto init = std::make_unique<LiteralExpr>(int64_t(-1));
    aria::VarDeclStmt stmt("trit", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors for trit value -1");
}

TEST_CASE(type_checker_trit_valid_zero) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // trit:x = 0;  // Valid
    auto init = std::make_unique<LiteralExpr>(int64_t(0));
    aria::VarDeclStmt stmt("trit", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors for trit value 0");
}

TEST_CASE(type_checker_trit_valid_positive) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // trit:x = 1;  // Valid
    auto init = std::make_unique<LiteralExpr>(int64_t(1));
    aria::VarDeclStmt stmt("trit", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors for trit value 1");
}

TEST_CASE(type_checker_trit_invalid_positive) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // trit:x = 2;  // Error: trit must be -1, 0, or 1
    auto init = std::make_unique<LiteralExpr>(int64_t(2));
    aria::VarDeclStmt stmt("trit", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error for invalid trit value 2");
}

TEST_CASE(type_checker_trit_invalid_negative) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // trit:x = -2;  // Error: trit must be -1, 0, or 1
    auto init = std::make_unique<LiteralExpr>(int64_t(-2));
    aria::VarDeclStmt stmt("trit", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error for invalid trit value -2");
}

TEST_CASE(type_checker_nit_valid_min) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // nit:x = -4;  // Valid: nit must be -4 to +4
    auto init = std::make_unique<LiteralExpr>(int64_t(-4));
    aria::VarDeclStmt stmt("nit", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors for nit value -4");
}

TEST_CASE(type_checker_nit_valid_zero) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // nit:x = 0;  // Valid
    auto init = std::make_unique<LiteralExpr>(int64_t(0));
    aria::VarDeclStmt stmt("nit", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors for nit value 0");
}

TEST_CASE(type_checker_nit_valid_max) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // nit:x = 4;  // Valid
    auto init = std::make_unique<LiteralExpr>(int64_t(4));
    aria::VarDeclStmt stmt("nit", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors for nit value 4");
}

TEST_CASE(type_checker_nit_invalid_positive) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // nit:x = 5;  // Error: nit must be -4 to +4
    auto init = std::make_unique<LiteralExpr>(int64_t(5));
    aria::VarDeclStmt stmt("nit", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error for invalid nit value 5");
}

TEST_CASE(type_checker_nit_invalid_negative) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // nit:x = -5;  // Error: nit must be -4 to +4
    auto init = std::make_unique<LiteralExpr>(int64_t(-5));
    aria::VarDeclStmt stmt("nit", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error for invalid nit value -5");
}

TEST_CASE(type_checker_tryte_valid_value) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // tryte:x = 1000;  // Valid: within [-29524, +29524]
    auto init = std::make_unique<LiteralExpr>(int64_t(1000));
    aria::VarDeclStmt stmt("tryte", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors for valid tryte value");
}

TEST_CASE(type_checker_tryte_out_of_range_positive) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // tryte:x = 30000;  // Error: out of range (max is +29524)
    auto init = std::make_unique<LiteralExpr>(int64_t(30000));
    aria::VarDeclStmt stmt("tryte", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error for out of range tryte value");
}

TEST_CASE(type_checker_tryte_out_of_range_negative) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // tryte:x = -30000;  // Error: out of range (min is -29524)
    auto init = std::make_unique<LiteralExpr>(int64_t(-30000));
    aria::VarDeclStmt stmt("tryte", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error for out of range tryte value");
}

TEST_CASE(type_checker_nyte_valid_value) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // nyte:x = -5000;  // Valid: within [-29524, +29524]
    auto init = std::make_unique<LiteralExpr>(int64_t(-5000));
    aria::VarDeclStmt stmt("nyte", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(!checker.hasErrors(), "Should have no errors for valid nyte value");
}

TEST_CASE(type_checker_nyte_out_of_range_positive) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // nyte:x = 30000;  // Error: out of range (max is +29524)
    auto init = std::make_unique<LiteralExpr>(int64_t(30000));
    aria::VarDeclStmt stmt("nyte", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error for out of range nyte value");
}

TEST_CASE(type_checker_balanced_no_coercion_from_int) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // int64:y = 1; trit:x = y;  // Error: cannot coerce int64 to trit
    auto int64Type = types.getPrimitiveType("int64");
    symbols.defineSymbol("y", SymbolKind::VARIABLE, int64Type);
    
    auto init = std::make_unique<IdentifierExpr>("y");
    aria::VarDeclStmt stmt("trit", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error (int64 → trit not allowed)");
}

TEST_CASE(type_checker_balanced_no_coercion_to_int) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // trit:y = 1; int64:x = y;  // Error: cannot coerce trit to int64
    auto tritType = types.getPrimitiveType("trit");
    symbols.defineSymbol("y", SymbolKind::VARIABLE, tritType);
    
    auto init = std::make_unique<IdentifierExpr>("y");
    aria::VarDeclStmt stmt("int64", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error (trit → int64 not allowed)");
}

TEST_CASE(type_checker_balanced_no_coercion_to_tbb) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // trit:y = 1; tbb8:x = y;  // Error: cannot coerce trit to tbb8
    auto tritType = types.getPrimitiveType("trit");
    symbols.defineSymbol("y", SymbolKind::VARIABLE, tritType);
    
    auto init = std::make_unique<IdentifierExpr>("y");
    aria::VarDeclStmt stmt("tbb8", "x", std::move(init));
    
    checker.checkStatement(&stmt);
    
    ASSERT(checker.hasErrors(), "Should have error (trit → tbb8 not allowed)");
}

TEST_CASE(type_checker_trit_assignment_valid) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // trit:x; x = 1;  // Valid
    auto tritType = types.getPrimitiveType("trit");
    symbols.defineSymbol("x", SymbolKind::VARIABLE, tritType);
    
    auto left = std::make_unique<IdentifierExpr>("x");
    auto right = std::make_unique<LiteralExpr>(int64_t(1));
    Token op(TokenType::TOKEN_EQUAL, "=", 1, 1);
    BinaryExpr expr(std::move(left), op, std::move(right));
    
    checker.checkAssignment(&expr);
    
    ASSERT(!checker.hasErrors(), "Should have no errors for valid trit assignment");
}

TEST_CASE(type_checker_nit_assignment_invalid) {
    TypeSystem types;
    SymbolTable symbols;
    TypeChecker checker(&types, &symbols);
    
    // nit:x; x = 10;  // Error: nit must be -4 to +4
    auto nitType = types.getPrimitiveType("nit");
    symbols.defineSymbol("x", SymbolKind::VARIABLE, nitType);
    
    auto left = std::make_unique<IdentifierExpr>("x");
    auto right = std::make_unique<LiteralExpr>(int64_t(10));
    Token op(TokenType::TOKEN_EQUAL, "=", 1, 1);
    BinaryExpr expr(std::move(left), op, std::move(right));
    
    checker.checkAssignment(&expr);
    
    ASSERT(checker.hasErrors(), "Should have error for invalid nit assignment");
}
