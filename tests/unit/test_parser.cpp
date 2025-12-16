#include "test_helpers.h"
#include "frontend/parser/parser.h"
#include "frontend/lexer/lexer.h"
#include "frontend/ast/ast_node.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/type.h"

using namespace aria;
using namespace aria::frontend;

// Helper to parse expression from source
ASTNodePtr parseExpr(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    return parser.parse();
}

// Helper to get first expression from program
ASTNodePtr getFirstExpr(ASTNodePtr program) {
    if (!program || program->type != ASTNode::NodeType::PROGRAM) {
        return nullptr;
    }
    auto prog = std::dynamic_pointer_cast<ProgramNode>(program);
    if (prog && !prog->declarations.empty()) {
        return prog->declarations[0];
    }
    return nullptr;
}

TEST_CASE(parser_primary_integer) {
    auto program = parseExpr("42");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::LITERAL, "Assertion failed");
    
    auto lit = std::dynamic_pointer_cast<LiteralExpr>(expr);
    ASSERT(std::holds_alternative<int64_t>(lit->value), "Value should be int64_t");
    ASSERT_EQ(std::get<int64_t>(lit->value), 42, "Assertion failed");
}

TEST_CASE(parser_primary_float) {
    auto program = parseExpr("3.14");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::LITERAL, "Assertion failed");
    
    auto lit = std::dynamic_pointer_cast<LiteralExpr>(expr);
    ASSERT(std::holds_alternative<double>(lit->value), "Value should be double");
    ASSERT(std::get<double>(lit->value) > 3.13 && std::get<double>(lit->value) < 3.15, "Float value should be approximately 3.14");
}

TEST_CASE(parser_primary_string) {
    auto program = parseExpr("\"hello\"");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::LITERAL, "Assertion failed");
    
    auto lit = std::dynamic_pointer_cast<LiteralExpr>(expr);
    ASSERT(std::holds_alternative<std::string>(lit->value), "Value should be string");
    ASSERT_EQ(std::get<std::string>(lit->value), "hello", "Assertion failed");
}

TEST_CASE(parser_primary_boolean_true) {
    auto program = parseExpr("true");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::LITERAL, "Assertion failed");
    
    auto lit = std::dynamic_pointer_cast<LiteralExpr>(expr);
    ASSERT(std::holds_alternative<bool>(lit->value), "Value should be bool");
    ASSERT_EQ(std::get<bool>(lit->value), true, "Assertion failed");
}

TEST_CASE(parser_primary_boolean_false) {
    auto program = parseExpr("false");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::LITERAL, "Assertion failed");
    
    auto lit = std::dynamic_pointer_cast<LiteralExpr>(expr);
    ASSERT(std::holds_alternative<bool>(lit->value), "Value should be bool");
    ASSERT_EQ(std::get<bool>(lit->value), false, "Assertion failed");
}

TEST_CASE(parser_primary_null) {
    auto program = parseExpr("NULL");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::LITERAL, "Assertion failed");
    
    auto lit = std::dynamic_pointer_cast<LiteralExpr>(expr);
    ASSERT(std::holds_alternative<std::monostate>(lit->value), "Value should be monostate");
}

TEST_CASE(parser_primary_identifier) {
    auto program = parseExpr("myVar");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::IDENTIFIER, "Assertion failed");
    
    auto ident = std::dynamic_pointer_cast<IdentifierExpr>(expr);
    ASSERT_EQ(ident->name, "myVar", "Assertion failed");
}

TEST_CASE(parser_primary_parenthesized) {
    auto program = parseExpr("(42)");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::LITERAL, "Assertion failed");
    
    auto lit = std::dynamic_pointer_cast<LiteralExpr>(expr);
    ASSERT_EQ(std::get<int64_t>(lit->value), 42, "Assertion failed");
}

TEST_CASE(parser_binary_addition) {
    auto program = parseExpr("10 + 20");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::BINARY_OP, "Assertion failed");
    
    auto binary = std::dynamic_pointer_cast<BinaryExpr>(expr);
    ASSERT_EQ(binary->op.type, TokenType::TOKEN_PLUS, "Assertion failed");
}

TEST_CASE(parser_binary_subtraction) {
    auto program = parseExpr("50 - 30");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::BINARY_OP, "Assertion failed");
    
    auto binary = std::dynamic_pointer_cast<BinaryExpr>(expr);
    ASSERT_EQ(binary->op.type, TokenType::TOKEN_MINUS, "Assertion failed");
}

TEST_CASE(parser_binary_multiplication) {
    auto program = parseExpr("5 * 6");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::BINARY_OP, "Assertion failed");
    
    auto binary = std::dynamic_pointer_cast<BinaryExpr>(expr);
    ASSERT_EQ(binary->op.type, TokenType::TOKEN_STAR, "Assertion failed");
}

TEST_CASE(parser_binary_division) {
    auto program = parseExpr("100 / 4");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::BINARY_OP, "Assertion failed");
    
    auto binary = std::dynamic_pointer_cast<BinaryExpr>(expr);
    ASSERT_EQ(binary->op.type, TokenType::TOKEN_SLASH, "Assertion failed");
}

TEST_CASE(parser_precedence_mult_before_add) {
    auto program = parseExpr("2 + 3 * 4");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::BINARY_OP, "Assertion failed");
    
    auto binary = std::dynamic_pointer_cast<BinaryExpr>(expr);
    ASSERT_EQ(binary->op.type, TokenType::TOKEN_PLUS, "Assertion failed");
    
    // Right side should be multiplication
    ASSERT_EQ(binary->right->type, ASTNode::NodeType::BINARY_OP, "Assertion failed");
    auto rightBinary = std::dynamic_pointer_cast<BinaryExpr>(binary->right);
    ASSERT_EQ(rightBinary->op.type, TokenType::TOKEN_STAR, "Assertion failed");
}

TEST_CASE(parser_precedence_parentheses) {
    auto program = parseExpr("(2 + 3) * 4");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::BINARY_OP, "Assertion failed");
    
    auto binary = std::dynamic_pointer_cast<BinaryExpr>(expr);
    ASSERT_EQ(binary->op.type, TokenType::TOKEN_STAR, "Assertion failed");
    
    // Left side should be addition
    ASSERT_EQ(binary->left->type, ASTNode::NodeType::BINARY_OP, "Assertion failed");
    auto leftBinary = std::dynamic_pointer_cast<BinaryExpr>(binary->left);
    ASSERT_EQ(leftBinary->op.type, TokenType::TOKEN_PLUS, "Assertion failed");
}

TEST_CASE(parser_unary_minus) {
    auto program = parseExpr("-42");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::UNARY_OP, "Assertion failed");
    
    auto unary = std::dynamic_pointer_cast<UnaryExpr>(expr);
    ASSERT_EQ(unary->op.type, TokenType::TOKEN_MINUS, "Assertion failed");
    ASSERT_EQ(unary->isPostfix, false, "Assertion failed");
}

TEST_CASE(parser_unary_not) {
    auto program = parseExpr("!true");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::UNARY_OP, "Assertion failed");
    
    auto unary = std::dynamic_pointer_cast<UnaryExpr>(expr);
    ASSERT_EQ(unary->op.type, TokenType::TOKEN_BANG, "Assertion failed");
    ASSERT_EQ(unary->isPostfix, false, "Assertion failed");
}

TEST_CASE(parser_unary_bitwise_not) {
    auto program = parseExpr("~value");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::UNARY_OP, "Assertion failed");
    
    auto unary = std::dynamic_pointer_cast<UnaryExpr>(expr);
    ASSERT_EQ(unary->op.type, TokenType::TOKEN_TILDE, "Assertion failed");
}

TEST_CASE(parser_call_no_args) {
    auto program = parseExpr("func()");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::CALL, "Assertion failed");
    
    auto call = std::dynamic_pointer_cast<CallExpr>(expr);
    ASSERT_EQ(call->callee->type, ASTNode::NodeType::IDENTIFIER, "Assertion failed");
    ASSERT_EQ(call->arguments.size(), 0, "Assertion failed");
}

TEST_CASE(parser_call_one_arg) {
    auto program = parseExpr("func(42)");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::CALL, "Assertion failed");
    
    auto call = std::dynamic_pointer_cast<CallExpr>(expr);
    ASSERT_EQ(call->arguments.size(), 1, "Assertion failed");
}

TEST_CASE(parser_call_multiple_args) {
    auto program = parseExpr("func(1, 2, 3)");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::CALL, "Assertion failed");
    
    auto call = std::dynamic_pointer_cast<CallExpr>(expr);
    ASSERT_EQ(call->arguments.size(), 3, "Assertion failed");
}

TEST_CASE(parser_index_access) {
    auto program = parseExpr("arr[5]");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::INDEX, "Assertion failed");
    
    auto index = std::dynamic_pointer_cast<IndexExpr>(expr);
    ASSERT_EQ(index->array->type, ASTNode::NodeType::IDENTIFIER, "Assertion failed");
}

TEST_CASE(parser_member_access) {
    auto program = parseExpr("obj.field");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::MEMBER_ACCESS, "Assertion failed");
    
    auto member = std::dynamic_pointer_cast<MemberAccessExpr>(expr);
    ASSERT_EQ(member->member, "field", "Assertion failed");
    ASSERT_EQ(member->isPointerAccess, false, "Assertion failed");
}

TEST_CASE(parser_pointer_member_access) {
    auto program = parseExpr("ptr->field");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::POINTER_MEMBER, "Assertion failed");
    
    auto member = std::dynamic_pointer_cast<MemberAccessExpr>(expr);
    ASSERT_EQ(member->member, "field", "Assertion failed");
    ASSERT_EQ(member->isPointerAccess, true, "Assertion failed");
}

TEST_CASE(parser_array_literal_empty) {
    auto program = parseExpr("[]");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::ARRAY_LITERAL, "Assertion failed");
    
    auto arr = std::dynamic_pointer_cast<ArrayLiteralExpr>(expr);
    ASSERT_EQ(arr->elements.size(), 0, "Assertion failed");
}

TEST_CASE(parser_array_literal_with_elements) {
    auto program = parseExpr("[1, 2, 3]");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::ARRAY_LITERAL, "Assertion failed");
    
    auto arr = std::dynamic_pointer_cast<ArrayLiteralExpr>(expr);
    ASSERT_EQ(arr->elements.size(), 3, "Assertion failed");
}

TEST_CASE(parser_complex_expression) {
    auto program = parseExpr("a + b * c - d / e");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::BINARY_OP, "Assertion failed");
    
    // Should parse as: (a + (b * c)) - (d / e)
    auto topLevel = std::dynamic_pointer_cast<BinaryExpr>(expr);
    ASSERT_EQ(topLevel->op.type, TokenType::TOKEN_MINUS, "Assertion failed");
}

TEST_CASE(parser_chained_calls) {
    auto program = parseExpr("obj.method().field");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::MEMBER_ACCESS, "Assertion failed");
}

TEST_CASE(parser_comparison) {
    auto program = parseExpr("x < 10");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::BINARY_OP, "Assertion failed");
    
    auto binary = std::dynamic_pointer_cast<BinaryExpr>(expr);
    ASSERT_EQ(binary->op.type, TokenType::TOKEN_LESS, "Assertion failed");
}

TEST_CASE(parser_logical_and) {
    auto program = parseExpr("a && b");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::BINARY_OP, "Assertion failed");
    
    auto binary = std::dynamic_pointer_cast<BinaryExpr>(expr);
    ASSERT_EQ(binary->op.type, TokenType::TOKEN_AND_AND, "Assertion failed");
}

TEST_CASE(parser_logical_or) {
    auto program = parseExpr("a || b");
    auto expr = getFirstExpr(program);
    
    ASSERT(expr != nullptr, "Expression should not be null");
    ASSERT_EQ(expr->type, ASTNode::NodeType::BINARY_OP, "Assertion failed");
    
    auto binary = std::dynamic_pointer_cast<BinaryExpr>(expr);
    ASSERT_EQ(binary->op.type, TokenType::TOKEN_OR_OR, "Assertion failed");
}

// ============================================================================
// PHASE 2.4: STATEMENT PARSING TESTS
// ============================================================================

// Helper to parse statements (multiple statements in a program)
ASTNodePtr parseStmt(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    return parser.parse();
}

// Helper to get program node
ProgramNode* getProgram(ASTNodePtr node) {
    if (!node || node->type != ASTNode::NodeType::PROGRAM) {
        return nullptr;
    }
    return dynamic_cast<ProgramNode*>(node.get());
}

// 2.4.1: Expression Statement Tests
TEST_CASE(parser_expression_statement) {
    auto program = parseStmt("x + 5;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have one statement");
    
    auto stmt = prog->declarations[0];
    ASSERT_EQ(stmt->type, ASTNode::NodeType::EXPRESSION_STMT, "Should be expression statement");
    
    auto exprStmt = std::dynamic_pointer_cast<ExpressionStmt>(stmt);
    ASSERT(exprStmt != nullptr, "Cast to ExpressionStmt should succeed");
    ASSERT(exprStmt->expression != nullptr, "Expression should not be null");
    ASSERT_EQ(exprStmt->expression->type, ASTNode::NodeType::BINARY_OP, "Should be binary expression");
}

TEST_CASE(parser_function_call_statement) {
    auto program = parseStmt("print(42);");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have one statement");
    
    auto stmt = prog->declarations[0];
    ASSERT_EQ(stmt->type, ASTNode::NodeType::EXPRESSION_STMT, "Should be expression statement");
    
    auto exprStmt = std::dynamic_pointer_cast<ExpressionStmt>(stmt);
    ASSERT_EQ(exprStmt->expression->type, ASTNode::NodeType::CALL, "Expression should be function call");
}

// 2.4.1: Block Statement Tests
TEST_CASE(parser_empty_block) {
    auto program = parseStmt("{}");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have one statement");
    
    auto stmt = prog->declarations[0];
    ASSERT_EQ(stmt->type, ASTNode::NodeType::BLOCK, "Should be block statement");
    
    auto block = std::dynamic_pointer_cast<BlockStmt>(stmt);
    ASSERT(block != nullptr, "Cast to BlockStmt should succeed");
    ASSERT_EQ(block->statements.size(), 0, "Block should be empty");
}

TEST_CASE(parser_block_with_statements) {
    auto program = parseStmt("{ x + 5; y * 2; }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have one statement");
    
    auto stmt = prog->declarations[0];
    ASSERT_EQ(stmt->type, ASTNode::NodeType::BLOCK, "Should be block statement");
    
    auto block = std::dynamic_pointer_cast<BlockStmt>(stmt);
    ASSERT(block != nullptr, "Cast to BlockStmt should succeed");
    ASSERT_EQ(block->statements.size(), 2, "Block should have two statements");
    
    ASSERT_EQ(block->statements[0]->type, ASTNode::NodeType::EXPRESSION_STMT, "First should be expression statement");
    ASSERT_EQ(block->statements[1]->type, ASTNode::NodeType::EXPRESSION_STMT, "Second should be expression statement");
}

// 2.4.1: Variable Declaration Tests
TEST_CASE(parser_var_decl_simple) {
    auto program = parseStmt("int8:x;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have one statement");
    
    auto stmt = prog->declarations[0];
    ASSERT_EQ(stmt->type, ASTNode::NodeType::VAR_DECL, "Should be variable declaration");
    
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(stmt);
    ASSERT(varDecl != nullptr, "Cast to VarDeclStmt should succeed");
    ASSERT_EQ(varDecl->typeName, "int8", "Type should be int8");
    ASSERT_EQ(varDecl->varName, "x", "Variable name should be x");
    ASSERT(varDecl->initializer == nullptr, "Should have no initializer");
}

TEST_CASE(parser_var_decl_with_init) {
    auto program = parseStmt("int8:x = 42;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have one statement");
    
    auto stmt = prog->declarations[0];
    ASSERT_EQ(stmt->type, ASTNode::NodeType::VAR_DECL, "Should be variable declaration");
    
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(stmt);
    ASSERT(varDecl != nullptr, "Cast to VarDeclStmt should succeed");
    ASSERT_EQ(varDecl->typeName, "int8", "Type should be int8");
    ASSERT_EQ(varDecl->varName, "x", "Variable name should be x");
    ASSERT(varDecl->initializer != nullptr, "Should have initializer");
    ASSERT_EQ(varDecl->initializer->type, ASTNode::NodeType::LITERAL, "Initializer should be literal");
}

TEST_CASE(parser_var_decl_string) {
    auto program = parseStmt("string:message = \"hello\";");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(stmt);
    ASSERT(varDecl != nullptr, "Cast to VarDeclStmt should succeed");
    ASSERT_EQ(varDecl->typeName, "string", "Type should be string");
    ASSERT_EQ(varDecl->varName, "message", "Variable name should be message");
    ASSERT(varDecl->initializer != nullptr, "Should have initializer");
}

TEST_CASE(parser_var_decl_wild) {
    auto program = parseStmt("wild int8:x = 10;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(stmt);
    ASSERT(varDecl != nullptr, "Cast to VarDeclStmt should succeed");
    ASSERT_EQ(varDecl->typeName, "int8", "Type should be int8");
    ASSERT_EQ(varDecl->varName, "x", "Variable name should be x");
    ASSERT(varDecl->isWild, "Should have wild qualifier");
    ASSERT(!varDecl->isConst, "Should not have const qualifier");
}

TEST_CASE(parser_var_decl_const) {
    auto program = parseStmt("const int8:x = 5;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(stmt);
    ASSERT(varDecl != nullptr, "Cast to VarDeclStmt should succeed");
    ASSERT(!varDecl->isWild, "Should not have wild qualifier");
    ASSERT(varDecl->isConst, "Should have const qualifier");
}

// 2.4.8: Return Statement Tests
TEST_CASE(parser_return_void) {
    auto program = parseStmt("return;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have one statement");
    
    auto stmt = prog->declarations[0];
    ASSERT_EQ(stmt->type, ASTNode::NodeType::RETURN, "Should be return statement");
    
    auto ret = std::dynamic_pointer_cast<ReturnStmt>(stmt);
    ASSERT(ret != nullptr, "Cast to ReturnStmt should succeed");
    ASSERT(ret->value == nullptr, "Should have no return value");
}

TEST_CASE(parser_return_with_value) {
    auto program = parseStmt("return 42;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto ret = std::dynamic_pointer_cast<ReturnStmt>(stmt);
    ASSERT(ret != nullptr, "Cast to ReturnStmt should succeed");
    ASSERT(ret->value != nullptr, "Should have return value");
    ASSERT_EQ(ret->value->type, ASTNode::NodeType::LITERAL, "Return value should be literal");
}

TEST_CASE(parser_return_expression) {
    auto program = parseStmt("return x + y;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto ret = std::dynamic_pointer_cast<ReturnStmt>(stmt);
    ASSERT(ret != nullptr, "Cast to ReturnStmt should succeed");
    ASSERT(ret->value != nullptr, "Should have return value");
    ASSERT_EQ(ret->value->type, ASTNode::NodeType::BINARY_OP, "Return value should be binary expression");
}

// Multiple statements in program
TEST_CASE(parser_multiple_statements) {
    auto program = parseStmt("int8:x = 10; int8:y = 20; x + y;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 3, "Should have three statements");
    
    ASSERT_EQ(prog->declarations[0]->type, ASTNode::NodeType::VAR_DECL, "First should be var decl");
    ASSERT_EQ(prog->declarations[1]->type, ASTNode::NodeType::VAR_DECL, "Second should be var decl");
    ASSERT_EQ(prog->declarations[2]->type, ASTNode::NodeType::EXPRESSION_STMT, "Third should be expression statement");
}

// ============================================================================
// If/Else Statement Tests (Phase 2.4.3)
// ============================================================================

// Simple if statement without else
TEST_CASE(parser_if_simple) {
    auto program = parseStmt("if (x > 5) { print(x); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto ifStmt = std::dynamic_pointer_cast<IfStmt>(stmt);
    ASSERT(ifStmt != nullptr, "Cast to IfStmt should succeed");
    ASSERT(ifStmt->condition != nullptr, "Condition should not be null");
    ASSERT(ifStmt->thenBranch != nullptr, "Then branch should not be null");
    ASSERT(ifStmt->elseBranch == nullptr, "Else branch should be null");
    
    ASSERT_EQ(ifStmt->condition->type, ASTNode::NodeType::BINARY_OP, "Condition should be binary op");
    ASSERT_EQ(ifStmt->thenBranch->type, ASTNode::NodeType::BLOCK, "Then branch should be block");
}

// If with else
TEST_CASE(parser_if_else) {
    auto program = parseStmt("if (x > 5) { print(x); } else { print(0); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto ifStmt = std::dynamic_pointer_cast<IfStmt>(stmt);
    ASSERT(ifStmt != nullptr, "Cast to IfStmt should succeed");
    ASSERT(ifStmt->condition != nullptr, "Condition should not be null");
    ASSERT(ifStmt->thenBranch != nullptr, "Then branch should not be null");
    ASSERT(ifStmt->elseBranch != nullptr, "Else branch should not be null");
    
    ASSERT_EQ(ifStmt->thenBranch->type, ASTNode::NodeType::BLOCK, "Then branch should be block");
    ASSERT_EQ(ifStmt->elseBranch->type, ASTNode::NodeType::BLOCK, "Else branch should be block");
}

// If-else if-else chain
TEST_CASE(parser_if_else_if) {
    auto program = parseStmt("if (x > 10) { print(1); } else if (x > 5) { print(2); } else { print(3); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto ifStmt = std::dynamic_pointer_cast<IfStmt>(stmt);
    ASSERT(ifStmt != nullptr, "Cast to IfStmt should succeed");
    ASSERT(ifStmt->condition != nullptr, "Condition should not be null");
    ASSERT(ifStmt->thenBranch != nullptr, "Then branch should not be null");
    ASSERT(ifStmt->elseBranch != nullptr, "Else branch should not be null");
    
    // The else branch should be another IfStmt (else if)
    auto elseIf = std::dynamic_pointer_cast<IfStmt>(ifStmt->elseBranch);
    ASSERT(elseIf != nullptr, "Else branch should be another IfStmt");
    ASSERT(elseIf->condition != nullptr, "Else if condition should not be null");
    ASSERT(elseIf->thenBranch != nullptr, "Else if then branch should not be null");
    ASSERT(elseIf->elseBranch != nullptr, "Else if else branch should not be null");
    
    ASSERT_EQ(elseIf->elseBranch->type, ASTNode::NodeType::BLOCK, "Final else should be block");
}

// If with single statement (no braces)
TEST_CASE(parser_if_single_statement) {
    auto program = parseStmt("if (x) return 1;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto ifStmt = std::dynamic_pointer_cast<IfStmt>(stmt);
    ASSERT(ifStmt != nullptr, "Cast to IfStmt should succeed");
    ASSERT(ifStmt->condition != nullptr, "Condition should not be null");
    ASSERT(ifStmt->thenBranch != nullptr, "Then branch should not be null");
    ASSERT(ifStmt->elseBranch == nullptr, "Else branch should be null");
    
    ASSERT_EQ(ifStmt->thenBranch->type, ASTNode::NodeType::RETURN, "Then branch should be return statement");
}

// If-else with single statements
TEST_CASE(parser_if_else_single_statements) {
    auto program = parseStmt("if (x) return 1; else return 0;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto ifStmt = std::dynamic_pointer_cast<IfStmt>(stmt);
    ASSERT(ifStmt != nullptr, "Cast to IfStmt should succeed");
    ASSERT(ifStmt->condition != nullptr, "Condition should not be null");
    ASSERT(ifStmt->thenBranch != nullptr, "Then branch should not be null");
    ASSERT(ifStmt->elseBranch != nullptr, "Else branch should not be null");
    
    ASSERT_EQ(ifStmt->thenBranch->type, ASTNode::NodeType::RETURN, "Then branch should be return");
    ASSERT_EQ(ifStmt->elseBranch->type, ASTNode::NodeType::RETURN, "Else branch should be return");
}

// Nested if statements
TEST_CASE(parser_if_nested) {
    auto program = parseStmt("if (x > 0) { if (y > 0) { print(1); } }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto outerIf = std::dynamic_pointer_cast<IfStmt>(stmt);
    ASSERT(outerIf != nullptr, "Outer if should be IfStmt");
    ASSERT(outerIf->thenBranch != nullptr, "Outer then branch should not be null");
    
    auto thenBlock = std::dynamic_pointer_cast<BlockStmt>(outerIf->thenBranch);
    ASSERT(thenBlock != nullptr, "Then branch should be block");
    ASSERT_EQ(thenBlock->statements.size(), 1, "Block should have one statement");
    
    auto innerIf = std::dynamic_pointer_cast<IfStmt>(thenBlock->statements[0]);
    ASSERT(innerIf != nullptr, "Inner statement should be IfStmt");
    ASSERT(innerIf->condition != nullptr, "Inner if should have condition");
}

// Complex condition
TEST_CASE(parser_if_complex_condition) {
    auto program = parseStmt("if (x > 5 && y < 10 || z == 0) { print(x); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto ifStmt = std::dynamic_pointer_cast<IfStmt>(stmt);
    ASSERT(ifStmt != nullptr, "Cast to IfStmt should succeed");
    ASSERT(ifStmt->condition != nullptr, "Condition should not be null");
    ASSERT_EQ(ifStmt->condition->type, ASTNode::NodeType::BINARY_OP, "Condition should be binary op");
}

// ============================================================================
// While Loop Tests (Phase 2.4.4)
// ============================================================================

// Simple while loop
TEST_CASE(parser_while_simple) {
    auto program = parseStmt("while (i < 100) { i++; }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(stmt);
    ASSERT(whileStmt != nullptr, "Cast to WhileStmt should succeed");
    ASSERT(whileStmt->condition != nullptr, "Condition should not be null");
    ASSERT(whileStmt->body != nullptr, "Body should not be null");
    
    ASSERT_EQ(whileStmt->condition->type, ASTNode::NodeType::BINARY_OP, "Condition should be binary op");
    ASSERT_EQ(whileStmt->body->type, ASTNode::NodeType::BLOCK, "Body should be block");
}

// While loop with single statement
TEST_CASE(parser_while_single_statement) {
    auto program = parseStmt("while (x) x++;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(stmt);
    ASSERT(whileStmt != nullptr, "Cast to WhileStmt should succeed");
    ASSERT(whileStmt->condition != nullptr, "Condition should not be null");
    ASSERT(whileStmt->body != nullptr, "Body should not be null");
    ASSERT_EQ(whileStmt->body->type, ASTNode::NodeType::EXPRESSION_STMT, "Body should be expression statement");
}

// While with complex condition
TEST_CASE(parser_while_complex_condition) {
    auto program = parseStmt("while (x > 0 && y < 100) { x--; y++; }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(stmt);
    ASSERT(whileStmt != nullptr, "Cast to WhileStmt should succeed");
    ASSERT(whileStmt->condition != nullptr, "Condition should not be null");
    ASSERT_EQ(whileStmt->condition->type, ASTNode::NodeType::BINARY_OP, "Condition should be binary op");
}

// Nested while loop
TEST_CASE(parser_while_nested) {
    auto program = parseStmt("while (i < 10) { while (j < 5) { j++; } i++; }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto outerWhile = std::dynamic_pointer_cast<WhileStmt>(stmt);
    ASSERT(outerWhile != nullptr, "Outer while should be WhileStmt");
    ASSERT(outerWhile->body != nullptr, "Outer while should have body");
    
    auto bodyBlock = std::dynamic_pointer_cast<BlockStmt>(outerWhile->body);
    ASSERT(bodyBlock != nullptr, "Body should be block");
    ASSERT_EQ(bodyBlock->statements.size(), 2, "Block should have two statements");
    
    auto innerWhile = std::dynamic_pointer_cast<WhileStmt>(bodyBlock->statements[0]);
    ASSERT(innerWhile != nullptr, "First statement should be WhileStmt");
}

// ============================================================================
// For Loop Tests (Phase 2.4.4)
// ============================================================================

// Simple for loop
TEST_CASE(parser_for_simple) {
    auto program = parseStmt("for (int8:i = 0; i < 100; i++) { print(i); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto forStmt = std::dynamic_pointer_cast<ForStmt>(stmt);
    ASSERT(forStmt != nullptr, "Cast to ForStmt should succeed");
    ASSERT(forStmt->initializer != nullptr, "Initializer should not be null");
    ASSERT(forStmt->condition != nullptr, "Condition should not be null");
    ASSERT(forStmt->update != nullptr, "Update should not be null");
    ASSERT(forStmt->body != nullptr, "Body should not be null");
    
    ASSERT_EQ(forStmt->initializer->type, ASTNode::NodeType::VAR_DECL, "Initializer should be var decl");
    ASSERT_EQ(forStmt->condition->type, ASTNode::NodeType::BINARY_OP, "Condition should be binary op");
    ASSERT_EQ(forStmt->body->type, ASTNode::NodeType::BLOCK, "Body should be block");
}

// For loop with existing variable
TEST_CASE(parser_for_existing_variable) {
    auto program = parseStmt("for (i = 0; i < 10; i++) { print(i); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto forStmt = std::dynamic_pointer_cast<ForStmt>(stmt);
    ASSERT(forStmt != nullptr, "Cast to ForStmt should succeed");
    ASSERT(forStmt->initializer != nullptr, "Initializer should not be null");
    ASSERT_EQ(forStmt->initializer->type, ASTNode::NodeType::BINARY_OP, "Initializer should be assignment");
}

// For loop with empty clauses
TEST_CASE(parser_for_infinite) {
    auto program = parseStmt("for (;;) { break; }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto forStmt = std::dynamic_pointer_cast<ForStmt>(stmt);
    ASSERT(forStmt != nullptr, "Cast to ForStmt should succeed");
    ASSERT(forStmt->initializer == nullptr, "Initializer should be null");
    ASSERT(forStmt->condition == nullptr, "Condition should be null");
    ASSERT(forStmt->update == nullptr, "Update should be null");
    ASSERT(forStmt->body != nullptr, "Body should not be null");
}

// For loop with single statement body
TEST_CASE(parser_for_single_statement) {
    auto program = parseStmt("for (int8:i = 0; i < 10; i++) sum += i;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto forStmt = std::dynamic_pointer_cast<ForStmt>(stmt);
    ASSERT(forStmt != nullptr, "Cast to ForStmt should succeed");
    ASSERT(forStmt->body != nullptr, "Body should not be null");
    ASSERT_EQ(forStmt->body->type, ASTNode::NodeType::EXPRESSION_STMT, "Body should be expression statement");
}

// Nested for loop
TEST_CASE(parser_for_nested) {
    auto program = parseStmt("for (int8:i = 0; i < 10; i++) { for (int8:j = 0; j < 5; j++) { print(j); } }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto outerFor = std::dynamic_pointer_cast<ForStmt>(stmt);
    ASSERT(outerFor != nullptr, "Outer for should be ForStmt");
    ASSERT(outerFor->body != nullptr, "Outer for should have body");
    
    auto bodyBlock = std::dynamic_pointer_cast<BlockStmt>(outerFor->body);
    ASSERT(bodyBlock != nullptr, "Body should be block");
    ASSERT_EQ(bodyBlock->statements.size(), 1, "Block should have one statement");
    
    auto innerFor = std::dynamic_pointer_cast<ForStmt>(bodyBlock->statements[0]);
    ASSERT(innerFor != nullptr, "Inner statement should be ForStmt");
}

// =========================================================================
// Break/Continue Tests
// =========================================================================

// Simple unlabeled break in while loop
TEST_CASE(parser_break_simple) {
    auto program = parseStmt("while (true) { break; }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(stmt);
    ASSERT(whileStmt != nullptr, "Should be WhileStmt");
    
    auto body = std::dynamic_pointer_cast<BlockStmt>(whileStmt->body);
    ASSERT(body != nullptr, "Body should be block");
    ASSERT_EQ(body->statements.size(), 1, "Block should have one statement");
    
    auto breakStmt = std::dynamic_pointer_cast<BreakStmt>(body->statements[0]);
    ASSERT(breakStmt != nullptr, "Should be BreakStmt");
    ASSERT(breakStmt->label.empty(), "Break should be unlabeled");
}

// Labeled break in nested loops
TEST_CASE(parser_break_labeled) {
    auto program = parseStmt("while (x > 0) { while (y > 0) { break(outer); } }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto outerWhile = std::dynamic_pointer_cast<WhileStmt>(stmt);
    ASSERT(outerWhile != nullptr, "Should be WhileStmt");
    
    auto outerBody = std::dynamic_pointer_cast<BlockStmt>(outerWhile->body);
    ASSERT(outerBody != nullptr, "Outer body should be block");
    
    auto innerWhile = std::dynamic_pointer_cast<WhileStmt>(outerBody->statements[0]);
    ASSERT(innerWhile != nullptr, "Inner should be WhileStmt");
    
    auto innerBody = std::dynamic_pointer_cast<BlockStmt>(innerWhile->body);
    ASSERT(innerBody != nullptr, "Inner body should be block");
    
    auto breakStmt = std::dynamic_pointer_cast<BreakStmt>(innerBody->statements[0]);
    ASSERT(breakStmt != nullptr, "Should be BreakStmt");
    ASSERT_EQ(breakStmt->label, "outer", "Break should have label 'outer'");
}

// Break with single statement body
TEST_CASE(parser_break_single_statement) {
    auto program = parseStmt("while (true) break;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(stmt);
    ASSERT(whileStmt != nullptr, "Should be WhileStmt");
    
    auto breakStmt = std::dynamic_pointer_cast<BreakStmt>(whileStmt->body);
    ASSERT(breakStmt != nullptr, "Body should be BreakStmt");
    ASSERT(breakStmt->label.empty(), "Break should be unlabeled");
}

// Simple unlabeled continue in for loop
TEST_CASE(parser_continue_simple) {
    auto program = parseStmt("for (int8:i = 0; i < 10; i++) { continue; }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto forStmt = std::dynamic_pointer_cast<ForStmt>(stmt);
    ASSERT(forStmt != nullptr, "Should be ForStmt");
    
    auto body = std::dynamic_pointer_cast<BlockStmt>(forStmt->body);
    ASSERT(body != nullptr, "Body should be block");
    ASSERT_EQ(body->statements.size(), 1, "Block should have one statement");
    
    auto continueStmt = std::dynamic_pointer_cast<ContinueStmt>(body->statements[0]);
    ASSERT(continueStmt != nullptr, "Should be ContinueStmt");
    ASSERT(continueStmt->label.empty(), "Continue should be unlabeled");
}

// Labeled continue in nested loops
TEST_CASE(parser_continue_labeled) {
    auto program = parseStmt("for (int8:i = 0; i < 10; i++) { for (int8:j = 0; j < 5; j++) { continue(outer); } }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto outerFor = std::dynamic_pointer_cast<ForStmt>(stmt);
    ASSERT(outerFor != nullptr, "Should be ForStmt");
    
    auto outerBody = std::dynamic_pointer_cast<BlockStmt>(outerFor->body);
    ASSERT(outerBody != nullptr, "Outer body should be block");
    
    auto innerFor = std::dynamic_pointer_cast<ForStmt>(outerBody->statements[0]);
    ASSERT(innerFor != nullptr, "Inner should be ForStmt");
    
    auto innerBody = std::dynamic_pointer_cast<BlockStmt>(innerFor->body);
    ASSERT(innerBody != nullptr, "Inner body should be block");
    
    auto continueStmt = std::dynamic_pointer_cast<ContinueStmt>(innerBody->statements[0]);
    ASSERT(continueStmt != nullptr, "Should be ContinueStmt");
    ASSERT_EQ(continueStmt->label, "outer", "Continue should have label 'outer'");
}

// Continue with single statement body
TEST_CASE(parser_continue_single_statement) {
    auto program = parseStmt("for (int8:i = 0; i < 10; i++) continue;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto forStmt = std::dynamic_pointer_cast<ForStmt>(stmt);
    ASSERT(forStmt != nullptr, "Should be ForStmt");
    
    auto continueStmt = std::dynamic_pointer_cast<ContinueStmt>(forStmt->body);
    ASSERT(continueStmt != nullptr, "Body should be ContinueStmt");
    ASSERT(continueStmt->label.empty(), "Continue should be unlabeled");
}

// Break and continue in same loop
TEST_CASE(parser_break_continue_combined) {
    auto program = parseStmt("while (x > 0) { if (done) break; if (skip) continue; process(); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(stmt);
    ASSERT(whileStmt != nullptr, "Should be WhileStmt");
    
    auto body = std::dynamic_pointer_cast<BlockStmt>(whileStmt->body);
    ASSERT(body != nullptr, "Body should be block");
    ASSERT_EQ(body->statements.size(), 3, "Block should have three statements");
    
    auto firstIf = std::dynamic_pointer_cast<IfStmt>(body->statements[0]);
    ASSERT(firstIf != nullptr, "First statement should be IfStmt");
    auto breakStmt = std::dynamic_pointer_cast<BreakStmt>(firstIf->thenBranch);
    ASSERT(breakStmt != nullptr, "Then branch should be BreakStmt");
    
    auto secondIf = std::dynamic_pointer_cast<IfStmt>(body->statements[1]);
    ASSERT(secondIf != nullptr, "Second statement should be IfStmt");
    auto continueStmt = std::dynamic_pointer_cast<ContinueStmt>(secondIf->thenBranch);
    ASSERT(continueStmt != nullptr, "Then branch should be ContinueStmt");
}

// Deeply nested break/continue
TEST_CASE(parser_break_continue_nested) {
    auto program = parseStmt("for (int8:i = 0; i < 10; i++) { while (check()) { if (done) { break(outer); } else { continue; } } }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto forStmt = std::dynamic_pointer_cast<ForStmt>(stmt);
    ASSERT(forStmt != nullptr, "Should be ForStmt");
    
    auto forBody = std::dynamic_pointer_cast<BlockStmt>(forStmt->body);
    ASSERT(forBody != nullptr, "For body should be block");
    
    auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(forBody->statements[0]);
    ASSERT(whileStmt != nullptr, "Should have while inside for");
    
    auto whileBody = std::dynamic_pointer_cast<BlockStmt>(whileStmt->body);
    ASSERT(whileBody != nullptr, "While body should be block");
    
    auto ifStmt = std::dynamic_pointer_cast<IfStmt>(whileBody->statements[0]);
    ASSERT(ifStmt != nullptr, "Should have if inside while");
    
    auto thenBlock = std::dynamic_pointer_cast<BlockStmt>(ifStmt->thenBranch);
    ASSERT(thenBlock != nullptr, "Then branch should be block");
    auto breakStmt = std::dynamic_pointer_cast<BreakStmt>(thenBlock->statements[0]);
    ASSERT(breakStmt != nullptr, "Should have break in then branch");
    ASSERT_EQ(breakStmt->label, "outer", "Break should target outer loop");
    
    auto elseBlock = std::dynamic_pointer_cast<BlockStmt>(ifStmt->elseBranch);
    ASSERT(elseBlock != nullptr, "Else branch should be block");
    auto continueStmt = std::dynamic_pointer_cast<ContinueStmt>(elseBlock->statements[0]);
    ASSERT(continueStmt != nullptr, "Should have continue in else branch");
    ASSERT(continueStmt->label.empty(), "Continue should be unlabeled");
}

// =========================================================================
// Till/Loop/When Tests (Aria-specific loops)
// =========================================================================

// Simple till loop counting up
TEST_CASE(parser_till_simple) {
    auto program = parseStmt("till(10, 1) { print($); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto tillStmt = std::dynamic_pointer_cast<TillStmt>(stmt);
    ASSERT(tillStmt != nullptr, "Should be TillStmt");
    ASSERT(tillStmt->limit != nullptr, "Limit should not be null");
    ASSERT(tillStmt->step != nullptr, "Step should not be null");
    ASSERT(tillStmt->body != nullptr, "Body should not be null");
}

// Till loop counting down
TEST_CASE(parser_till_negative_step) {
    auto program = parseStmt("till(100, -1) { process($); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto tillStmt = std::dynamic_pointer_cast<TillStmt>(stmt);
    ASSERT(tillStmt != nullptr, "Should be TillStmt");
    ASSERT(tillStmt->body != nullptr, "Body should not be null");
}

// Simple loop with start/limit/step
TEST_CASE(parser_loop_simple) {
    auto program = parseStmt("loop(1, 100, 1) { print($); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto loopStmt = std::dynamic_pointer_cast<LoopStmt>(stmt);
    ASSERT(loopStmt != nullptr, "Should be LoopStmt");
    ASSERT(loopStmt->start != nullptr, "Start should not be null");
    ASSERT(loopStmt->limit != nullptr, "Limit should not be null");
    ASSERT(loopStmt->step != nullptr, "Step should not be null");
    ASSERT(loopStmt->body != nullptr, "Body should not be null");
}

// Loop counting down (start > limit)
TEST_CASE(parser_loop_countdown) {
    auto program = parseStmt("loop(100, 0, 2) { countdown($); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto loopStmt = std::dynamic_pointer_cast<LoopStmt>(stmt);
    ASSERT(loopStmt != nullptr, "Should be LoopStmt");
    ASSERT(loopStmt->body != nullptr, "Body should not be null");
}

// Nested till loops ($ shadowing)
TEST_CASE(parser_till_nested) {
    auto program = parseStmt("till(10, 1) { till(5, 1) { print($); } }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto outerTill = std::dynamic_pointer_cast<TillStmt>(stmt);
    ASSERT(outerTill != nullptr, "Outer should be TillStmt");
    
    auto outerBody = std::dynamic_pointer_cast<BlockStmt>(outerTill->body);
    ASSERT(outerBody != nullptr, "Outer body should be block");
    ASSERT_EQ(outerBody->statements.size(), 1, "Outer body should have one statement");
    
    auto innerTill = std::dynamic_pointer_cast<TillStmt>(outerBody->statements[0]);
    ASSERT(innerTill != nullptr, "Inner should be TillStmt");
}

// Simple when loop (body only)
TEST_CASE(parser_when_simple) {
    auto program = parseStmt("when(x < 10) { x++; }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto whenStmt = std::dynamic_pointer_cast<WhenStmt>(stmt);
    ASSERT(whenStmt != nullptr, "Should be WhenStmt");
    ASSERT(whenStmt->condition != nullptr, "Condition should not be null");
    ASSERT(whenStmt->body != nullptr, "Body should not be null");
    ASSERT(whenStmt->then_block == nullptr, "Then block should be null");
    ASSERT(whenStmt->end_block == nullptr, "End block should be null");
}

// When loop with then block
TEST_CASE(parser_when_with_then) {
    auto program = parseStmt("when(x < 10) { x++; } then { print(\"done\"); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto whenStmt = std::dynamic_pointer_cast<WhenStmt>(stmt);
    ASSERT(whenStmt != nullptr, "Should be WhenStmt");
    ASSERT(whenStmt->condition != nullptr, "Condition should not be null");
    ASSERT(whenStmt->body != nullptr, "Body should not be null");
    ASSERT(whenStmt->then_block != nullptr, "Then block should not be null");
    ASSERT(whenStmt->end_block == nullptr, "End block should be null");
}

// When loop with end block
TEST_CASE(parser_when_with_end) {
    auto program = parseStmt("when(searching) { if (found) break; } end { notFound(); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto whenStmt = std::dynamic_pointer_cast<WhenStmt>(stmt);
    ASSERT(whenStmt != nullptr, "Should be WhenStmt");
    ASSERT(whenStmt->condition != nullptr, "Condition should not be null");
    ASSERT(whenStmt->body != nullptr, "Body should not be null");
    ASSERT(whenStmt->then_block == nullptr, "Then block should be null");
    ASSERT(whenStmt->end_block != nullptr, "End block should not be null");
}

// When loop with both then and end blocks
TEST_CASE(parser_when_complete) {
    auto program = parseStmt("when(x > 0) { x--; } then { success(); } end { failure(); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto whenStmt = std::dynamic_pointer_cast<WhenStmt>(stmt);
    ASSERT(whenStmt != nullptr, "Should be WhenStmt");
    ASSERT(whenStmt->condition != nullptr, "Condition should not be null");
    ASSERT(whenStmt->body != nullptr, "Body should not be null");
    ASSERT(whenStmt->then_block != nullptr, "Then block should not be null");
    ASSERT(whenStmt->end_block != nullptr, "End block should not be null");
}

// Till with break
TEST_CASE(parser_till_with_break) {
    auto program = parseStmt("till(100, 1) { if (i == 50) break; }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto tillStmt = std::dynamic_pointer_cast<TillStmt>(stmt);
    ASSERT(tillStmt != nullptr, "Should be TillStmt");
    
    auto body = std::dynamic_pointer_cast<BlockStmt>(tillStmt->body);
    ASSERT(body != nullptr, "Body should be block");
    ASSERT_EQ(body->statements.size(), 1, "Body should have one statement");
    
    auto ifStmt = std::dynamic_pointer_cast<IfStmt>(body->statements[0]);
    ASSERT(ifStmt != nullptr, "Should have if statement");
}

// Loop with complex expressions
TEST_CASE(parser_loop_complex_expressions) {
    auto program = parseStmt("loop(0, 10, 1) { x++; }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() > 0, "Program should have at least one declaration");
    auto stmt = prog->declarations[0];
    
    auto loopStmt = std::dynamic_pointer_cast<LoopStmt>(stmt);
    ASSERT(loopStmt != nullptr, "Should be LoopStmt");
    ASSERT(loopStmt->start != nullptr, "Start should not be null");
    ASSERT(loopStmt->limit != nullptr, "Limit should not be null");
    ASSERT(loopStmt->step != nullptr, "Step should not be null");
}

// ============================================================================
// Pick Statement Tests (Phase 2.4.7)
// ============================================================================

TEST_CASE(parser_pick_simple) {
    auto program = parseStmt("pick(x) { (5) { print(x); } }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() > 0, "Program should have at least one declaration");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::ASTNode::NodeType::PICK, "Should be a PICK statement");
    
    auto pickStmt = std::static_pointer_cast<PickStmt>(stmt);
    ASSERT(pickStmt->selector != nullptr, "Selector should not be null");
    ASSERT(pickStmt->cases.size() == 1, "Should have 1 case");
}

TEST_CASE(parser_pick_multiple_cases) {
    auto program = parseStmt("pick(value) { (5) { first(); }, (10) { second(); }, (20) { third(); } }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() > 0, "Program should have declarations");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::PICK, "Should be PICK statement");
    
    auto pickStmt = std::static_pointer_cast<PickStmt>(stmt);
    ASSERT(pickStmt->cases.size() == 3, "Should have 3 cases");
}

TEST_CASE(parser_pick_with_wildcard) {
    auto program = parseStmt("pick(status) { (200) { ok(); }, (404) { notFound(); }, (*) { other(); } }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::PICK, "Should be PICK statement");
    
    auto pickStmt = std::static_pointer_cast<PickStmt>(stmt);
    ASSERT(pickStmt->cases.size() == 3, "Should have 3 cases");
    
    // Check wildcard case
    auto wildcardCase = std::static_pointer_cast<PickCase>(pickStmt->cases[2]);
    ASSERT(wildcardCase->pattern != nullptr, "Wildcard pattern should not be null");
}

TEST_CASE(parser_pick_with_single_label) {
    std::string code = "pick(c) { success:(9) { doSuccess(); } }";
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();
    
    if (parser.hasErrors()) {
        std::cout << "Parser errors:\n";
        for (const auto& err : parser.getErrors()) {
            std::cout << "  " << err << "\n";
        }
    }
    
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() > 0, "Program should have declarations");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::PICK, "Should be PICK statement");
    
    auto pickStmt = std::static_pointer_cast<PickStmt>(stmt);
    ASSERT(pickStmt->cases.size() == 1, "Should have 1 case");
}

TEST_CASE(parser_pick_with_labels) {
    std::string code = "pick(c) { success:(9) { doSuccess(); }, fail:(5) { doFail(); } }";
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();
    
    if (parser.hasErrors()) {
        std::cout << "Parser errors:\n";
        for (const auto& err : parser.getErrors()) {
            std::cout << "  " << err << "\n";
        }
    }
    
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() > 0, "Program should have declarations");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::PICK, "Should be PICK statement");
    
    auto pickStmt = std::static_pointer_cast<PickStmt>(stmt);
    ASSERT(pickStmt->cases.size() == 2, "Should have 2 cases");
}

TEST_CASE(parser_pick_with_unreachable) {
    auto program = parseStmt("pick(x) { (5) { normal(); }, fail:(!) { unreachable(); } }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::PICK, "Should be PICK statement");
    
    auto pickStmt = std::static_pointer_cast<PickStmt>(stmt);
    ASSERT(pickStmt->cases.size() == 2, "Should have 2 cases");
    
    auto unreachableCase = std::static_pointer_cast<PickCase>(pickStmt->cases[1]);
    ASSERT(unreachableCase->is_unreachable == true, "Second case should be unreachable");
    ASSERT(unreachableCase->label == "fail", "Unreachable case should have label");
}

TEST_CASE(parser_pick_with_expressions) {
    auto program = parseStmt("pick(value) { (10) { a(); }, (20) { b(); }, (30) { c(); } }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::PICK, "Should be PICK statement");
    
    auto pickStmt = std::static_pointer_cast<PickStmt>(stmt);
    ASSERT(pickStmt->cases.size() == 3, "Should have 3 cases");
}

TEST_CASE(parser_fall_statement) {
    auto program = parseStmt("fall(done);");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() > 0, "Program should have declarations");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::FALL, "Should be FALL statement");
    
    auto fallStmt = std::static_pointer_cast<FallStmt>(stmt);
    ASSERT(fallStmt->target_label == "done", "Target label should be 'done'");
}

TEST_CASE(parser_pick_with_fall) {
    auto program = parseStmt("pick(x) { (5) { fall(fail); }, (9) { fall(success); }, fail:(!) { error(); }, success:(!) { ok(); } }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::PICK, "Should be PICK statement");
    
    auto pickStmt = std::static_pointer_cast<PickStmt>(stmt);
    ASSERT(pickStmt->cases.size() == 4, "Should have 4 cases");
}

TEST_CASE(parser_pick_nested) {
    auto program = parseStmt("pick(x) { (1) { pick(y) { (2) { nested(); } } }, (*) { other(); } }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::PICK, "Should be PICK statement");
    
    auto pickStmt = std::static_pointer_cast<PickStmt>(stmt);
    ASSERT(pickStmt->cases.size() == 2, "Should have 2 cases");
}
// =============================================================================
// PHASE 2.4.8: Defer Statement Tests (Block-Scoped RAII)
// =============================================================================
// Research: research_020 - Control Transfer (defer section)
// Syntax: defer { block }
// Semantics: Block-scoped RAII cleanup, executes at scope exit in LIFO order

TEST_CASE(parser_defer_simple) {
    auto program = parseStmt("defer { cleanup(); }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() > 0, "Program should have at least one declaration");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::DEFER, "Should be a DEFER statement");
    
    auto deferStmt = std::static_pointer_cast<DeferStmt>(stmt);
    ASSERT(deferStmt->block != nullptr, "Defer block should not be null");
    ASSERT(deferStmt->block->type == ASTNode::NodeType::BLOCK, "Defer should contain a BLOCK");
}

TEST_CASE(parser_defer_multiple_statements) {
    auto program = parseStmt("defer { free(ptr); close(file); unlock(mutex); }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() > 0, "Program should have declarations");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::DEFER, "Should be DEFER statement");
    
    auto deferStmt = std::static_pointer_cast<DeferStmt>(stmt);
    ASSERT(deferStmt->block != nullptr, "Defer block should not be null");
    auto blockStmt = std::static_pointer_cast<BlockStmt>(deferStmt->block);
    ASSERT(blockStmt->statements.size() == 3, "Defer block should have 3 statements");
}

TEST_CASE(parser_defer_with_variable_capture) {
    auto program = parseStmt("defer { aria.free(ptr); }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::DEFER, "Should be DEFER statement");
    
    auto deferStmt = std::static_pointer_cast<DeferStmt>(stmt);
    ASSERT(deferStmt->block != nullptr, "Block should capture variable reference");
}

TEST_CASE(parser_defer_empty_block) {
    auto program = parseStmt("defer { }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::DEFER, "Should be DEFER statement");
    
    auto deferStmt = std::static_pointer_cast<DeferStmt>(stmt);
    ASSERT(deferStmt->block != nullptr, "Block should exist even if empty");
}

TEST_CASE(parser_defer_inside_if) {
    auto program = parseStmt("if (condition) { defer { cleanup(); } }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::IF, "Should be IF statement");
    
    auto ifStmt = std::static_pointer_cast<IfStmt>(stmt);
    auto thenBlock = std::static_pointer_cast<BlockStmt>(ifStmt->thenBranch);
    ASSERT(thenBlock->statements.size() > 0, "Then block should have statements");
    auto deferStmt = std::static_pointer_cast<DeferStmt>(thenBlock->statements[0]);
    ASSERT(deferStmt->type == ASTNode::NodeType::DEFER, "First statement should be defer");
}

TEST_CASE(parser_defer_inside_loop) {
    auto program = parseStmt("while (hasMore) { defer { releaseResource(); } }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::WHILE, "Should be WHILE statement");
    
    auto whileStmt = std::static_pointer_cast<WhileStmt>(stmt);
    auto bodyBlock = std::static_pointer_cast<BlockStmt>(whileStmt->body);
    ASSERT(bodyBlock->statements.size() > 0, "Loop body should have statements");
}

TEST_CASE(parser_defer_multiple_in_scope) {
    auto program = parseStmt("{ defer { first(); } defer { second(); } defer { third(); } }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::BLOCK, "Should be BLOCK statement");
    
    auto blockStmt = std::static_pointer_cast<BlockStmt>(stmt);
    ASSERT(blockStmt->statements.size() == 3, "Block should have 3 defer statements");
    
    // All three should be defer statements (LIFO execution order at scope exit)
    for (size_t i = 0; i < 3; i++) {
        ASSERT(blockStmt->statements[i]->type == ASTNode::NodeType::DEFER,
               "Statement should be DEFER");
    }
}

TEST_CASE(parser_defer_nested_blocks) {
    auto program = parseStmt("defer { { nested(); } }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::DEFER, "Should be DEFER statement");
    
    auto deferStmt = std::static_pointer_cast<DeferStmt>(stmt);
    auto outerBlock = std::static_pointer_cast<BlockStmt>(deferStmt->block);
    ASSERT(outerBlock->statements.size() > 0, "Outer block should have statements");
}

TEST_CASE(parser_defer_with_return) {
    auto program = parseStmt("{ defer { cleanup(); } return value; }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::BLOCK, "Should be BLOCK statement");
    
    auto blockStmt = std::static_pointer_cast<BlockStmt>(stmt);
    ASSERT(blockStmt->statements.size() == 2, "Block should have defer and return");
    ASSERT(blockStmt->statements[0]->type == ASTNode::NodeType::DEFER, "First should be defer");
    ASSERT(blockStmt->statements[1]->type == ASTNode::NodeType::RETURN, "Second should be return");
}

// =============================================================================
// PHASE 2.4.9: Pass/Fail Statements (Result Monad Integration)
// =============================================================================
// Research: research_020 - Control Transfer (pass/fail section)
// Syntax: pass(expr); and fail(error_code);
// Semantics: Syntactic sugar for result type construction and return
//   pass(x) → return { err: 0, val: x }
//   fail(e) → return { err: e, val: 0 }

TEST_CASE(parser_pass_simple) {
    auto program = parseStmt("pass(42);");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() > 0, "Program should have declarations");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::RETURN, "Pass should desugar to RETURN");
    
    auto returnStmt = std::static_pointer_cast<ReturnStmt>(stmt);
    ASSERT(returnStmt->value != nullptr, "Return should have value");
}

TEST_CASE(parser_pass_expression) {
    auto program = parseStmt("pass(x + 10);");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::RETURN, "Should desugar to RETURN");
    
    auto returnStmt = std::static_pointer_cast<ReturnStmt>(stmt);
    ASSERT(returnStmt->value != nullptr, "Should have result object");
}

TEST_CASE(parser_pass_variable) {
    auto program = parseStmt("pass(value);");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() > 0, "Program should have declarations");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::RETURN, "Should be RETURN statement");
}

TEST_CASE(parser_pass_function_call) {
    auto program = parseStmt("pass(computeValue(a, b));");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() > 0, "Program should have declarations");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::RETURN, "Should be RETURN statement");
    
    auto returnStmt = std::static_pointer_cast<ReturnStmt>(stmt);
    ASSERT(returnStmt->value != nullptr, "Should have value");
}

TEST_CASE(parser_fail_simple) {
    auto program = parseStmt("fail(1);");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() > 0, "Program should have declarations");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::RETURN, "Fail should desugar to RETURN");
    
    auto returnStmt = std::static_pointer_cast<ReturnStmt>(stmt);
    ASSERT(returnStmt->value != nullptr, "Return should have value");
}

TEST_CASE(parser_fail_error_code) {
    auto program = parseStmt("fail(errorCode);");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() > 0, "Program should have declarations");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::RETURN, "Should desugar to RETURN");
}

TEST_CASE(parser_fail_expression) {
    auto program = parseStmt("fail(ERR_NOT_FOUND);");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() > 0, "Program should have declarations");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::RETURN, "Should be RETURN statement");
}

TEST_CASE(parser_pass_in_if) {
    auto program = parseStmt("if (valid) { pass(value); }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::IF, "Should be IF statement");
    
    auto ifStmt = std::static_pointer_cast<IfStmt>(stmt);
    auto thenBlock = std::static_pointer_cast<BlockStmt>(ifStmt->thenBranch);
    ASSERT(thenBlock->statements.size() > 0, "Then block should have statements");
    ASSERT(thenBlock->statements[0]->type == ASTNode::NodeType::RETURN, 
           "Pass should desugar to return");
}

TEST_CASE(parser_fail_in_else) {
    auto program = parseStmt("if (valid) { pass(x); } else { fail(1); }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::IF, "Should be IF statement");
    
    auto ifStmt = std::static_pointer_cast<IfStmt>(stmt);
    ASSERT(ifStmt->elseBranch != nullptr, "Should have else branch");
    auto elseBlock = std::static_pointer_cast<BlockStmt>(ifStmt->elseBranch);
    ASSERT(elseBlock->statements[0]->type == ASTNode::NodeType::RETURN,
           "Fail should desugar to return");
}

TEST_CASE(parser_pass_fail_pattern) {
    auto program = parseStmt("{ if (success) { pass(value); } fail(errCode); }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::BLOCK, "Should be BLOCK");
    
    auto block = std::static_pointer_cast<BlockStmt>(stmt);
    ASSERT(block->statements.size() == 2, "Block should have if and fail");
}

TEST_CASE(parser_pass_with_defer) {
    auto program = parseStmt("{ defer { cleanup(); } pass(value); }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::BLOCK, "Should be BLOCK");
    
    auto block = std::static_pointer_cast<BlockStmt>(stmt);
    ASSERT(block->statements.size() == 2, "Should have defer and pass");
    ASSERT(block->statements[0]->type == ASTNode::NodeType::DEFER, "First is defer");
    ASSERT(block->statements[1]->type == ASTNode::NodeType::RETURN, "Second is pass→return");
}

TEST_CASE(parser_nested_pass_fail) {
    auto program = parseStmt("if (check1) { if (check2) { pass(val); } fail(2); }");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    ASSERT(stmt->type == ASTNode::NodeType::IF, "Should be IF statement");
}
// ═══════════════════════════════════════════════════════════════════════
// Phase 2.4.2: Function Declaration Tests
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE(parser_func_no_params) {
    auto program = parseStmt("func:getName = string() { return \"test\"; };");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have one declaration");
    
    auto stmt = prog->declarations[0];
    ASSERT_EQ(stmt->type, ASTNode::NodeType::FUNC_DECL, "Should be function declaration");
    
    auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(stmt);
    ASSERT(funcDecl != nullptr, "Cast to FuncDeclStmt should succeed");
    ASSERT_EQ(funcDecl->funcName, "getName", "Function name should be getName");
    ASSERT_EQ(funcDecl->returnType, "string", "Return type should be string");
    ASSERT_EQ(funcDecl->parameters.size(), 0, "Should have no parameters");
    ASSERT(funcDecl->body != nullptr, "Should have a body");
}

TEST_CASE(parser_func_one_param) {
    auto program = parseStmt("func:double = int8(int8:x) { return x * 2; };");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(stmt);
    ASSERT(funcDecl != nullptr, "Cast to FuncDeclStmt should succeed");
    ASSERT_EQ(funcDecl->funcName, "double", "Function name should be double");
    ASSERT_EQ(funcDecl->returnType, "int8", "Return type should be int8");
    ASSERT_EQ(funcDecl->parameters.size(), 1, "Should have one parameter");
    
    auto param = std::dynamic_pointer_cast<ParameterNode>(funcDecl->parameters[0]);
    ASSERT(param != nullptr, "Cast to ParameterNode should succeed");
    ASSERT_EQ(param->typeName, "int8", "Parameter type should be int8");
    ASSERT_EQ(param->paramName, "x", "Parameter name should be x");
}

TEST_CASE(parser_func_multiple_params) {
    auto program = parseStmt("func:add = int32(int32:a, int32:b) { return a + b; };");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(stmt);
    ASSERT(funcDecl != nullptr, "Cast to FuncDeclStmt should succeed");
    ASSERT_EQ(funcDecl->funcName, "add", "Function name should be add");
    ASSERT_EQ(funcDecl->returnType, "int32", "Return type should be int32");
    ASSERT_EQ(funcDecl->parameters.size(), 2, "Should have two parameters");
    
    auto param1 = std::dynamic_pointer_cast<ParameterNode>(funcDecl->parameters[0]);
    ASSERT_EQ(param1->typeName, "int32", "First parameter type should be int32");
    ASSERT_EQ(param1->paramName, "a", "First parameter name should be a");
    
    auto param2 = std::dynamic_pointer_cast<ParameterNode>(funcDecl->parameters[1]);
    ASSERT_EQ(param2->typeName, "int32", "Second parameter type should be int32");
    ASSERT_EQ(param2->paramName, "b", "Second parameter name should be b");
}

TEST_CASE(parser_func_with_pass) {
    auto program = parseStmt("func:test = int8(int8:x) { pass(x * 2); };");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(stmt);
    ASSERT(funcDecl != nullptr, "Cast to FuncDeclStmt should succeed");
    ASSERT(funcDecl->body != nullptr, "Should have a body");
    
    auto block = std::dynamic_pointer_cast<BlockStmt>(funcDecl->body);
    ASSERT(block != nullptr, "Body should be a BlockStmt");
    ASSERT(block->statements.size() > 0, "Block should have statements");
}

TEST_CASE(parser_func_empty_body) {
    auto program = parseStmt("func:noop = int8() { };");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(stmt);
    ASSERT(funcDecl != nullptr, "Cast to FuncDeclStmt should succeed");
    ASSERT_EQ(funcDecl->funcName, "noop", "Function name should be noop");
    ASSERT_EQ(funcDecl->returnType, "int8", "Return type should be int8");
    ASSERT(funcDecl->body != nullptr, "Should have a body");
    
    auto block = std::dynamic_pointer_cast<BlockStmt>(funcDecl->body);
    ASSERT(block != nullptr, "Body should be a BlockStmt");
}

TEST_CASE(parser_func_complex_body) {
    auto program = parseStmt("func:calc = int64(int64:x, int64:y) { int64:sum = x + y; pass(sum); };");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(stmt);
    ASSERT(funcDecl != nullptr, "Cast to FuncDeclStmt should succeed");
    ASSERT_EQ(funcDecl->parameters.size(), 2, "Should have two parameters");
    
    auto block = std::dynamic_pointer_cast<BlockStmt>(funcDecl->body);
    ASSERT(block != nullptr, "Body should be a BlockStmt");
    ASSERT(block->statements.size() >= 1, "Block should have at least one statement");
}

TEST_CASE(parser_func_with_if) {
    auto program = parseStmt("func:abs = int8(int8:x) { if (x < 0) { pass(-x); } pass(x); };");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(stmt);
    ASSERT(funcDecl != nullptr, "Cast to FuncDeclStmt should succeed");
    ASSERT(funcDecl->body != nullptr, "Should have a body");
    
    auto block = std::dynamic_pointer_cast<BlockStmt>(funcDecl->body);
    ASSERT(block != nullptr, "Body should be a BlockStmt");
    ASSERT(block->statements.size() > 0, "Block should have statements");
}

TEST_CASE(parser_func_with_loop) {
    auto program = parseStmt("func:sum = int32(int32:n) { int32:total = 0; while (n > 0) { total = total + n; n = n - 1; } pass(total); };");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(stmt);
    ASSERT(funcDecl != nullptr, "Cast to FuncDeclStmt should succeed");
    ASSERT_EQ(funcDecl->funcName, "sum", "Function name should be sum");
    
    auto block = std::dynamic_pointer_cast<BlockStmt>(funcDecl->body);
    ASSERT(block != nullptr, "Body should be a BlockStmt");
    ASSERT(block->statements.size() >= 2, "Block should have multiple statements");
}

// ============================================================================
// Phase 2.5.1: Type Annotation Parsing Tests
// ============================================================================
// Note: Since parseType() is private, we test it indirectly through
// variable declarations that exercise the type parsing functionality

TEST_CASE(parser_type_simple_int8) {
    auto program = parseStmt("int8:x = 42;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(stmt);
    ASSERT(varDecl != nullptr, "Cast to VarDeclStmt should succeed");
    ASSERT_EQ(varDecl->typeName, "int8", "Type should be int8");
    ASSERT_EQ(varDecl->varName, "x", "Variable name should be x");
}

TEST_CASE(parser_type_simple_string) {
    auto program = parseStmt("string:name = \"test\";");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(stmt);
    ASSERT(varDecl != nullptr, "Cast to VarDeclStmt should succeed");
    ASSERT_EQ(varDecl->typeName, "string", "Type should be string");
    ASSERT_EQ(varDecl->varName, "name", "Variable name should be name");
}

TEST_CASE(parser_type_simple_bool) {
    auto program = parseStmt("bool:flag = true;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(stmt);
    ASSERT(varDecl != nullptr, "Cast to VarDeclStmt should succeed");
    ASSERT_EQ(varDecl->typeName, "bool", "Type should be bool");
    ASSERT_EQ(varDecl->varName, "flag", "Variable name should be flag");
}

TEST_CASE(parser_type_int32) {
    auto program = parseStmt("int32:count = 100;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(stmt);
    ASSERT(varDecl != nullptr, "Cast to VarDeclStmt should succeed");
    ASSERT_EQ(varDecl->typeName, "int32", "Type should be int32");
    ASSERT_EQ(varDecl->varName, "count", "Variable name should be count");
}

TEST_CASE(parser_type_int64) {
    auto program = parseStmt("int64:big = 9999;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(stmt);
    ASSERT(varDecl != nullptr, "Cast to VarDeclStmt should succeed");
    ASSERT_EQ(varDecl->typeName, "int64", "Type should be int64");
    ASSERT_EQ(varDecl->varName, "big", "Variable name should be big");
}

TEST_CASE(parser_type_flt32) {
    auto program = parseStmt("flt32:pi = 3.14;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(stmt);
    ASSERT(varDecl != nullptr, "Cast to VarDeclStmt should succeed");
    ASSERT_EQ(varDecl->typeName, "flt32", "Type should be flt32");
    ASSERT_EQ(varDecl->varName, "pi", "Variable name should be pi");
}

TEST_CASE(parser_type_in_function_params) {
    auto program = parseStmt("func:add = int32(int32:a, int32:b) { pass(a + b); };");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(stmt);
    ASSERT(funcDecl != nullptr, "Cast to FuncDeclStmt should succeed");
    ASSERT_EQ(funcDecl->funcName, "add", "Function name should be add");
    ASSERT_EQ(funcDecl->returnType, "int32", "Return type should be int32");
    ASSERT_EQ(funcDecl->parameters.size(), 2, "Should have 2 parameters");
}

TEST_CASE(parser_type_multiple_vars_same_type) {
    auto program = parseStmt("int8:x = 1; int8:y = 2; int8:z = 3;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() >= 3, "Should have at least 3 declarations");
    
    auto var1 = std::dynamic_pointer_cast<VarDeclStmt>(prog->declarations[0]);
    auto var2 = std::dynamic_pointer_cast<VarDeclStmt>(prog->declarations[1]);
    auto var3 = std::dynamic_pointer_cast<VarDeclStmt>(prog->declarations[2]);
    
    ASSERT(var1 != nullptr, "First should be VarDeclStmt");
    ASSERT(var2 != nullptr, "Second should be VarDeclStmt");
    ASSERT(var3 != nullptr, "Third should be VarDeclStmt");
    
    ASSERT_EQ(var1->typeName, "int8", "First type should be int8");
    ASSERT_EQ(var2->typeName, "int8", "Second type should be int8");
    ASSERT_EQ(var3->typeName, "int8", "Third type should be int8");
    
    ASSERT_EQ(var1->varName, "x", "First var should be x");
    ASSERT_EQ(var2->varName, "y", "Second var should be y");
    ASSERT_EQ(var3->varName, "z", "Third var should be z");
}

TEST_CASE(parser_type_mixed_types) {
    auto program = parseStmt("int8:x = 1; string:name = \"test\"; bool:flag = true;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() >= 3, "Should have at least 3 declarations");
    
    auto var1 = std::dynamic_pointer_cast<VarDeclStmt>(prog->declarations[0]);
    auto var2 = std::dynamic_pointer_cast<VarDeclStmt>(prog->declarations[1]);
    auto var3 = std::dynamic_pointer_cast<VarDeclStmt>(prog->declarations[2]);
    
    ASSERT(var1 != nullptr, "First should be VarDeclStmt");
    ASSERT(var2 != nullptr, "Second should be VarDeclStmt");
    ASSERT(var3 != nullptr, "Third should be VarDeclStmt");
    
    ASSERT_EQ(var1->typeName, "int8", "First type should be int8");
    ASSERT_EQ(var2->typeName, "string", "Second type should be string");
    ASSERT_EQ(var3->typeName, "bool", "Third type should be bool");
}

// ============================================================================
// Phase 2.5.2: use Statement Parsing Tests
// ============================================================================

TEST_CASE(parser_use_simple) {
    auto program = parseStmt("use std.io;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto useStmt = std::dynamic_pointer_cast<UseStmt>(stmt);
    ASSERT(useStmt != nullptr, "Cast to UseStmt should succeed");
    ASSERT_EQ(useStmt->path.size(), 2, "Path should have 2 segments");
    ASSERT_EQ(useStmt->path[0], "std", "First segment should be std");
    ASSERT_EQ(useStmt->path[1], "io", "Second segment should be io");
    ASSERT(!useStmt->isWildcard, "Should not be wildcard");
    ASSERT(useStmt->items.empty(), "Should not have selective items");
    ASSERT(useStmt->alias.empty(), "Should not have alias");
}

TEST_CASE(parser_use_nested_path) {
    auto program = parseStmt("use std.collections.map;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto useStmt = std::dynamic_pointer_cast<UseStmt>(stmt);
    ASSERT(useStmt != nullptr, "Cast to UseStmt should succeed");
    ASSERT_EQ(useStmt->path.size(), 3, "Path should have 3 segments");
    ASSERT_EQ(useStmt->path[0], "std", "First segment should be std");
    ASSERT_EQ(useStmt->path[1], "collections", "Second segment should be collections");
    ASSERT_EQ(useStmt->path[2], "map", "Third segment should be map");
}

TEST_CASE(parser_use_selective_single) {
    auto program = parseStmt("use std.collections.{array};");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto useStmt = std::dynamic_pointer_cast<UseStmt>(stmt);
    ASSERT(useStmt != nullptr, "Cast to UseStmt should succeed");
    ASSERT_EQ(useStmt->path.size(), 2, "Path should have 2 segments");
    ASSERT_EQ(useStmt->path[0], "std", "First segment should be std");
    ASSERT_EQ(useStmt->path[1], "collections", "Second segment should be collections");
    ASSERT_EQ(useStmt->items.size(), 1, "Should have 1 item");
    ASSERT_EQ(useStmt->items[0], "array", "Item should be array");
    ASSERT(!useStmt->isWildcard, "Should not be wildcard");
}

TEST_CASE(parser_use_selective_multiple) {
    // Test without spaces first to debug
    auto program = parseStmt("use std.collections.{array,map,Vector};");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto useStmt = std::dynamic_pointer_cast<UseStmt>(stmt);
    ASSERT(useStmt != nullptr, "Cast to UseStmt should succeed");
    ASSERT_EQ(useStmt->path.size(), 2, "Path should have 2 segments");
    ASSERT_EQ(useStmt->items.size(), 3, "Should have 3 items");
    ASSERT_EQ(useStmt->items[0], "array", "First item should be array");
    ASSERT_EQ(useStmt->items[1], "map", "Second item should be map");
    ASSERT_EQ(useStmt->items[2], "Vector", "Third item should be Vector");
}

TEST_CASE(parser_use_wildcard) {
    auto program = parseStmt("use math.*;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto useStmt = std::dynamic_pointer_cast<UseStmt>(stmt);
    ASSERT(useStmt != nullptr, "Cast to UseStmt should succeed");
    ASSERT_EQ(useStmt->path.size(), 1, "Path should have 1 segment");
    ASSERT_EQ(useStmt->path[0], "math", "Path should be math");
    ASSERT(useStmt->isWildcard, "Should be wildcard");
    ASSERT(useStmt->items.empty(), "Should not have selective items");
}

TEST_CASE(parser_use_file_path_relative) {
    auto program = parseStmt("use \"./utils.aria\";");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto useStmt = std::dynamic_pointer_cast<UseStmt>(stmt);
    ASSERT(useStmt != nullptr, "Cast to UseStmt should succeed");
    ASSERT(useStmt->isFilePath, "Should be a file path");
    ASSERT_EQ(useStmt->path.size(), 1, "Path should have 1 element");
    ASSERT_EQ(useStmt->path[0], "./utils.aria", "Path should be ./utils.aria");
    ASSERT(useStmt->alias.empty(), "Should not have alias");
}

TEST_CASE(parser_use_file_path_parent) {
    auto program = parseStmt("use \"../shared/crypto.aria\";");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto useStmt = std::dynamic_pointer_cast<UseStmt>(stmt);
    ASSERT(useStmt != nullptr, "Cast to UseStmt should succeed");
    ASSERT(useStmt->isFilePath, "Should be a file path");
    ASSERT_EQ(useStmt->path[0], "../shared/crypto.aria", "Path should be ../shared/crypto.aria");
}

TEST_CASE(parser_use_file_path_absolute) {
    auto program = parseStmt("use \"/usr/lib/aria/graphics\";");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto useStmt = std::dynamic_pointer_cast<UseStmt>(stmt);
    ASSERT(useStmt != nullptr, "Cast to UseStmt should succeed");
    ASSERT(useStmt->isFilePath, "Should be a file path");
    ASSERT_EQ(useStmt->path[0], "/usr/lib/aria/graphics", "Path should be absolute path");
}

TEST_CASE(parser_use_with_alias_file) {
    auto program = parseStmt("use \"./utils.aria\" as utils;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto useStmt = std::dynamic_pointer_cast<UseStmt>(stmt);
    ASSERT(useStmt != nullptr, "Cast to UseStmt should succeed");
    ASSERT(useStmt->isFilePath, "Should be a file path");
    ASSERT_EQ(useStmt->path[0], "./utils.aria", "Path should be ./utils.aria");
    ASSERT_EQ(useStmt->alias, "utils", "Alias should be utils");
}

TEST_CASE(parser_use_with_alias_module) {
    auto program = parseStmt("use std.network.http.client as HttpClient;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    auto stmt = prog->declarations[0];
    
    auto useStmt = std::dynamic_pointer_cast<UseStmt>(stmt);
    ASSERT(useStmt != nullptr, "Cast to UseStmt should succeed");
    ASSERT_EQ(useStmt->path.size(), 4, "Path should have 4 segments");
    ASSERT_EQ(useStmt->path[3], "client", "Last segment should be client");
    ASSERT_EQ(useStmt->alias, "HttpClient", "Alias should be HttpClient");
}

TEST_CASE(parser_use_multiple_statements) {
    auto program = parseStmt("use std.io; use std.collections.{array, map}; use math.*;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT(prog->declarations.size() >= 3, "Should have at least 3 declarations");
    
    auto use1 = std::dynamic_pointer_cast<UseStmt>(prog->declarations[0]);
    auto use2 = std::dynamic_pointer_cast<UseStmt>(prog->declarations[1]);
    auto use3 = std::dynamic_pointer_cast<UseStmt>(prog->declarations[2]);
    
    ASSERT(use1 != nullptr, "First should be UseStmt");
    ASSERT(use2 != nullptr, "Second should be UseStmt");
    ASSERT(use3 != nullptr, "Third should be UseStmt");
    
    ASSERT_EQ(use1->path[1], "io", "First use should import io");
    ASSERT_EQ(use2->items.size(), 2, "Second use should have 2 items");
    ASSERT(use3->isWildcard, "Third use should be wildcard");
}

// ============================================================================
// mod Statement Tests (Phase 2.5.3)
// ============================================================================

TEST_CASE(parser_mod_external) {
    auto program = parseStmt("mod network;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have 1 declaration");
    
    auto modStmt = std::dynamic_pointer_cast<ModStmt>(prog->declarations[0]);
    ASSERT(modStmt != nullptr, "Should be ModStmt");
    ASSERT_EQ(modStmt->name, "network", "Module name should be network");
    ASSERT(!modStmt->isPublic, "Should not be public");
    ASSERT(!modStmt->isInline, "Should not be inline");
}

TEST_CASE(parser_mod_public) {
    auto program = parseStmt("pub mod utils;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have 1 declaration");
    
    auto modStmt = std::dynamic_pointer_cast<ModStmt>(prog->declarations[0]);
    ASSERT(modStmt != nullptr, "Should be ModStmt");
    ASSERT_EQ(modStmt->name, "utils", "Module name should be utils");
    ASSERT(modStmt->isPublic, "Should be public");
    ASSERT(!modStmt->isInline, "Should not be inline");
}

TEST_CASE(parser_mod_inline_empty) {
    auto program = parseStmt("mod internal { }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have 1 declaration");
    
    auto modStmt = std::dynamic_pointer_cast<ModStmt>(prog->declarations[0]);
    ASSERT(modStmt != nullptr, "Should be ModStmt");
    ASSERT_EQ(modStmt->name, "internal", "Module name should be internal");
    ASSERT(!modStmt->isPublic, "Should not be public");
    ASSERT(modStmt->isInline, "Should be inline");
    ASSERT_EQ(modStmt->body.size(), 0, "Body should be empty");
}

TEST_CASE(parser_mod_inline_with_func) {
    auto program = parseStmt("mod math { func:add = int8(int8:a, int8:b) { return a + b; } }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have 1 declaration");
    
    auto modStmt = std::dynamic_pointer_cast<ModStmt>(prog->declarations[0]);
    ASSERT(modStmt != nullptr, "Should be ModStmt");
    ASSERT_EQ(modStmt->name, "math", "Module name should be math");
    ASSERT(modStmt->isInline, "Should be inline");
    ASSERT_EQ(modStmt->body.size(), 1, "Body should have 1 statement");
    
    auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(modStmt->body[0]);
    ASSERT(funcDecl != nullptr, "Body should contain function declaration");
    ASSERT_EQ(funcDecl->funcName, "add", "Function name should be add");
}

TEST_CASE(parser_mod_inline_with_multiple) {
    auto program = parseStmt("mod utils { int8:x = 5; int8:y = 10; }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have 1 declaration");
    
    auto modStmt = std::dynamic_pointer_cast<ModStmt>(prog->declarations[0]);
    ASSERT(modStmt != nullptr, "Should be ModStmt");
    ASSERT_EQ(modStmt->name, "utils", "Module name should be utils");
    ASSERT(modStmt->isInline, "Should be inline");
    ASSERT_EQ(modStmt->body.size(), 2, "Body should have 2 statements");
}

TEST_CASE(parser_mod_pub_inline) {
    auto program = parseStmt("pub mod helpers { int8:x = 42; }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have 1 declaration");
    
    auto modStmt = std::dynamic_pointer_cast<ModStmt>(prog->declarations[0]);
    ASSERT(modStmt != nullptr, "Should be ModStmt");
    ASSERT_EQ(modStmt->name, "helpers", "Module name should be helpers");
    ASSERT(modStmt->isPublic, "Should be public");
    ASSERT(modStmt->isInline, "Should be inline");
    ASSERT_EQ(modStmt->body.size(), 1, "Body should have 1 statement");
}

TEST_CASE(parser_mod_multiple) {
    auto program = parseStmt("mod network; mod ui; pub mod utils;");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 3, "Should have 3 declarations");
    
    auto mod1 = std::dynamic_pointer_cast<ModStmt>(prog->declarations[0]);
    auto mod2 = std::dynamic_pointer_cast<ModStmt>(prog->declarations[1]);
    auto mod3 = std::dynamic_pointer_cast<ModStmt>(prog->declarations[2]);
    
    ASSERT(mod1 != nullptr, "First should be ModStmt");
    ASSERT(mod2 != nullptr, "Second should be ModStmt");
    ASSERT(mod3 != nullptr, "Third should be ModStmt");
    
    ASSERT_EQ(mod1->name, "network", "First module should be network");
    ASSERT_EQ(mod2->name, "ui", "Second module should be ui");
    ASSERT_EQ(mod3->name, "utils", "Third module should be utils");
    ASSERT(mod3->isPublic, "Third module should be public");
}

// ============================================================================
// extern Statement Tests (Phase 2.5.4)
// ============================================================================

TEST_CASE(parser_extern_empty) {
    auto program = parseStmt("extern \"libc\" { }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have 1 declaration");
    
    auto externStmt = std::dynamic_pointer_cast<ExternStmt>(prog->declarations[0]);
    ASSERT(externStmt != nullptr, "Should be ExternStmt");
    ASSERT_EQ(externStmt->libraryName, "libc", "Library name should be libc");
    ASSERT_EQ(externStmt->declarations.size(), 0, "Should have no declarations");
}

TEST_CASE(parser_extern_with_func) {
    auto program = parseStmt("extern \"libc\" { func:malloc = void*(uint64:size); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have 1 declaration");
    
    auto externStmt = std::dynamic_pointer_cast<ExternStmt>(prog->declarations[0]);
    ASSERT(externStmt != nullptr, "Should be ExternStmt");
    ASSERT_EQ(externStmt->libraryName, "libc", "Library name should be libc");
    ASSERT_EQ(externStmt->declarations.size(), 1, "Should have 1 declaration");
    
    auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(externStmt->declarations[0]);
    ASSERT(funcDecl != nullptr, "Should contain function declaration");
    ASSERT_EQ(funcDecl->funcName, "malloc", "Function name should be malloc");
}

TEST_CASE(parser_extern_multiple_funcs) {
    auto program = parseStmt("extern \"libc\" { func:malloc = void*(uint64:size); func:free = void(void*:ptr); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have 1 declaration");
    
    auto externStmt = std::dynamic_pointer_cast<ExternStmt>(prog->declarations[0]);
    ASSERT(externStmt != nullptr, "Should be ExternStmt");
    ASSERT_EQ(externStmt->libraryName, "libc", "Library name should be libc");
    ASSERT_EQ(externStmt->declarations.size(), 2, "Should have 2 declarations");
    
    auto func1 = std::dynamic_pointer_cast<FuncDeclStmt>(externStmt->declarations[0]);
    auto func2 = std::dynamic_pointer_cast<FuncDeclStmt>(externStmt->declarations[1]);
    
    ASSERT(func1 != nullptr, "First should be function declaration");
    ASSERT(func2 != nullptr, "Second should be function declaration");
    ASSERT_EQ(func1->funcName, "malloc", "First function should be malloc");
    ASSERT_EQ(func2->funcName, "free", "Second function should be free");
}

TEST_CASE(parser_extern_with_variable) {
    auto program = parseStmt("extern \"libc\" { wild int32:errno; }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have 1 declaration");
    
    auto externStmt = std::dynamic_pointer_cast<ExternStmt>(prog->declarations[0]);
    ASSERT(externStmt != nullptr, "Should be ExternStmt");
    ASSERT_EQ(externStmt->libraryName, "libc", "Library name should be libc");
    ASSERT_EQ(externStmt->declarations.size(), 1, "Should have 1 declaration");
    
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(externStmt->declarations[0]);
    ASSERT(varDecl != nullptr, "Should contain variable declaration");
}

TEST_CASE(parser_extern_different_library) {
    auto program = parseStmt("extern \"kernel32\" { func:Sleep = void(uint32:dwMilliseconds); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have 1 declaration");
    
    auto externStmt = std::dynamic_pointer_cast<ExternStmt>(prog->declarations[0]);
    ASSERT(externStmt != nullptr, "Should be ExternStmt");
    ASSERT_EQ(externStmt->libraryName, "kernel32", "Library name should be kernel32");
    ASSERT_EQ(externStmt->declarations.size(), 1, "Should have 1 declaration");
}

TEST_CASE(parser_extern_mixed_declarations) {
    auto program = parseStmt("extern \"libc\" { func:printf = int32(string:format); wild int32:errno; func:exit = void(int32:code); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 1, "Should have 1 declaration");
    
    auto externStmt = std::dynamic_pointer_cast<ExternStmt>(prog->declarations[0]);
    ASSERT(externStmt != nullptr, "Should be ExternStmt");
    ASSERT_EQ(externStmt->libraryName, "libc", "Library name should be libc");
    ASSERT_EQ(externStmt->declarations.size(), 3, "Should have 3 declarations");
}

TEST_CASE(parser_extern_multiple_blocks) {
    auto program = parseStmt("extern \"libc\" { func:malloc = void*(uint64:size); } extern \"kernel32\" { func:Sleep = void(uint32:ms); }");
    auto prog = getProgram(program);
    
    ASSERT(prog != nullptr, "Program should not be null");
    ASSERT_EQ(prog->declarations.size(), 2, "Should have 2 declarations");
    
    auto extern1 = std::dynamic_pointer_cast<ExternStmt>(prog->declarations[0]);
    auto extern2 = std::dynamic_pointer_cast<ExternStmt>(prog->declarations[1]);
    
    ASSERT(extern1 != nullptr, "First should be ExternStmt");
    ASSERT(extern2 != nullptr, "Second should be ExternStmt");
    ASSERT_EQ(extern1->libraryName, "libc", "First library should be libc");
    ASSERT_EQ(extern2->libraryName, "kernel32", "Second library should be kernel32");
}

// ============================================================================
// Phase 2.5.1: Type Annotation Parsing Tests (Extended)
// ============================================================================
// Tests for parseType() through variable declarations and function signatures
// Type syntax: simple (int8), pointer (int8@), array (int8[], int8[100]), generic (Array<int8>)

// Simple type tests - basic primitives
TEST_CASE(parser_type_uint8) {
    auto program = parseStmt("uint8:byte = 255;");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(prog->declarations[0]);
    ASSERT(varDecl != nullptr, "Should be VarDeclStmt");
    ASSERT_EQ(varDecl->typeName, "uint8", "Type should be uint8");
}

TEST_CASE(parser_type_uint32) {
    auto program = parseStmt("uint32:count = 1000;");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(prog->declarations[0]);
    ASSERT(varDecl != nullptr, "Should be VarDeclStmt");
    ASSERT_EQ(varDecl->typeName, "uint32", "Type should be uint32");
}

TEST_CASE(parser_type_uint64) {
    auto program = parseStmt("uint64:bignum = 999;");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(prog->declarations[0]);
    ASSERT(varDecl != nullptr, "Should be VarDeclStmt");
    ASSERT_EQ(varDecl->typeName, "uint64", "Type should be uint64");
}

TEST_CASE(parser_type_flt64) {
    auto program = parseStmt("flt64:precise = 3.14159;");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(prog->declarations[0]);
    ASSERT(varDecl != nullptr, "Should be VarDeclStmt");
    ASSERT_EQ(varDecl->typeName, "flt64", "Type should be flt64");
}

// TBB (Twisted Balanced Binary) types
TEST_CASE(parser_type_tbb8) {
    auto program = parseStmt("tbb8:safe = 100;");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(prog->declarations[0]);
    ASSERT(varDecl != nullptr, "Should be VarDeclStmt");
    ASSERT_EQ(varDecl->typeName, "tbb8", "Type should be tbb8");
}

TEST_CASE(parser_type_tbb32) {
    auto program = parseStmt("tbb32:balanced = 500;");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(prog->declarations[0]);
    ASSERT(varDecl != nullptr, "Should be VarDeclStmt");
    ASSERT_EQ(varDecl->typeName, "tbb32", "Type should be tbb32");
}

// Composite types
TEST_CASE(parser_type_obj) {
    auto program = parseStmt("obj:config = { key: \"value\" };");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(prog->declarations[0]);
    ASSERT(varDecl != nullptr, "Should be VarDeclStmt");
    ASSERT_EQ(varDecl->typeName, "obj", "Type should be obj");
}

TEST_CASE(parser_type_dyn) {
    auto program = parseStmt("dyn:flexible = 42;");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto varDecl = std::dynamic_pointer_cast<VarDeclStmt>(prog->declarations[0]);
    ASSERT(varDecl != nullptr, "Should be VarDeclStmt");
    ASSERT_EQ(varDecl->typeName, "dyn", "Type should be dyn");
}

// Type in function parameters (multiple different types)
TEST_CASE(parser_type_func_params_varied) {
    auto program = parseStmt("func:process = int32(string:name, bool:flag, flt32:ratio) { pass(0); };");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(prog->declarations[0]);
    ASSERT(funcDecl != nullptr, "Should be FuncDeclStmt");
    ASSERT_EQ(funcDecl->parameters.size(), 3, "Should have 3 parameters");
    
    auto param1 = std::dynamic_pointer_cast<ParameterNode>(funcDecl->parameters[0]);
    auto param2 = std::dynamic_pointer_cast<ParameterNode>(funcDecl->parameters[1]);
    auto param3 = std::dynamic_pointer_cast<ParameterNode>(funcDecl->parameters[2]);
    
    ASSERT_EQ(param1->typeName, "string", "First param should be string");
    ASSERT_EQ(param2->typeName, "bool", "Second param should be bool");
    ASSERT_EQ(param3->typeName, "flt32", "Third param should be flt32");
}

// Function with different return types
TEST_CASE(parser_type_func_return_bool) {
    auto program = parseStmt("func:isValid = bool() { pass(true); };");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(prog->declarations[0]);
    ASSERT(funcDecl != nullptr, "Should be FuncDeclStmt");
    ASSERT_EQ(funcDecl->returnType, "bool", "Return type should be bool");
}

TEST_CASE(parser_type_func_return_string) {
    auto program = parseStmt("func:getName = string() { pass(\"test\"); };");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(prog->declarations[0]);
    ASSERT(funcDecl != nullptr, "Should be FuncDeclStmt");
    ASSERT_EQ(funcDecl->returnType, "string", "Return type should be string");
}

// Complex function with varied types
TEST_CASE(parser_type_func_complex) {
    auto program = parseStmt("func:calculate = flt64(int32:x, int32:y, flt32:factor) { pass(0.0); };");
    auto prog = getProgram(program);
    ASSERT(prog != nullptr, "Program should not be null");
    auto funcDecl = std::dynamic_pointer_cast<FuncDeclStmt>(prog->declarations[0]);
    ASSERT(funcDecl != nullptr, "Should be FuncDeclStmt");
    ASSERT_EQ(funcDecl->returnType, "flt64", "Return type should be flt64");
    ASSERT_EQ(funcDecl->parameters.size(), 3, "Should have 3 parameters");
}

// TODO: Add these tests once pointer/array/generic type parsing is integrated:
// - Pointer types: int8@, string@
// - Dynamic arrays: int8[], string[]
// - Sized arrays: int8[100], uint32[256]
// - Generic types: Array<int8>, Map<string, int32>
// - Nested generics: Array<Array<int32>>
// - Function types as parameters

// ============================================================================
// Parser Error Handling Tests (Phase 2.6)
// ============================================================================

// Test that parser collects errors instead of crashing
TEST_CASE(parser_error_missing_semicolon) {
    std::string source = "int32:x = 5";  // Missing semicolon
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    
    ASSERT(parser.hasErrors(), "Parser should have errors");
    ASSERT_EQ(parser.getErrors().size(), 1, "Should have 1 error");
}

TEST_CASE(parser_error_unclosed_paren) {
    std::string source = "int32:x = (5 + 3;";  // Missing closing paren
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    
    ASSERT(parser.hasErrors(), "Parser should have errors");
}

TEST_CASE(parser_error_missing_condition) {
    std::string source = "if { x = 5; };";  // Missing condition
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    
    ASSERT(parser.hasErrors(), "Parser should have errors");
}

TEST_CASE(parser_error_invalid_type) {
    std::string source = "invalidtype:x = 5;";  // Invalid type keyword
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    
    ASSERT(parser.hasErrors(), "Parser should have errors");
}

// Test error recovery - parser continues after error
TEST_CASE(parser_error_recovery_multiple_statements) {
    std::string source = R"(
        int32:x = 5;
        int32:y = (3 + 2;  // Error: unclosed paren
        int32:z = 10;      // Should still parse this
    )";
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    
    ASSERT(parser.hasErrors(), "Parser should have errors");
    // Parser should recover and parse the third statement
    auto prog = std::dynamic_pointer_cast<ProgramNode>(ast);
    ASSERT(prog != nullptr, "Should still create program node");
}

// Test that single error doesn't cause cascade
TEST_CASE(parser_error_no_cascade) {
    std::string source = R"(
        int32:x = ;  // Error: missing initializer
        int32:y = 10;
    )";
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    
    ASSERT(parser.hasErrors(), "Parser should have errors");
    // Should not have dozens of errors from the single mistake
    ASSERT(parser.getErrors().size() < 5, "Should not have cascading errors");
}

// Test error message format
TEST_CASE(parser_error_message_format) {
    std::string source = "int32:x = 5";  // Missing semicolon
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    
    ASSERT(parser.hasErrors(), "Parser should have errors");
    const auto& errors = parser.getErrors();
    ASSERT(!errors.empty(), "Should have at least one error");
    
    // Error message should contain line number
    std::string errorMsg = errors[0];
    ASSERT(errorMsg.find("line") != std::string::npos, "Error should mention line number");
}

// Test synchronization at semicolon
TEST_CASE(parser_error_sync_semicolon) {
    std::string source = R"(
        int32:x = (5 + 3;
        int32:y = 10;
    )";
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    
    // Parser should sync at semicolon and continue
    auto prog = std::dynamic_pointer_cast<ProgramNode>(ast);
    ASSERT(prog != nullptr, "Should create program node");
}

// Test synchronization at keyword
TEST_CASE(parser_error_sync_keyword) {
    std::string source = R"(
        int32:x = (5 + 3
        func:test = int32() { pass(42); };
    )";
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    
    // Parser should sync at 'func' keyword
    auto prog = std::dynamic_pointer_cast<ProgramNode>(ast);
    ASSERT(prog != nullptr, "Should create program node");
}
