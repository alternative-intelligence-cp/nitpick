#include "test_helpers.h"
#include "frontend/parser/parser.h"
#include "frontend/lexer/lexer.h"
#include "frontend/ast/stmt.h"

using namespace aria;
using namespace aria::frontend;

// ============================================================================
// Generic Syntax Parsing Tests - Phase 3.4
// ============================================================================

// Helper to parse source code
ASTNodePtr parseSource(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    return parser.parse();
}

// ----------------------------------------------------------------------------
// Generic Function Declaration Tests
// ----------------------------------------------------------------------------

TEST_CASE(parse_generic_function_simple) {
    std::string source = R"(
        func<T>:identity = *T(*T:value) {
            return value;
        };
    )";
    
    auto ast = parseSource(source);
    ASSERT(ast != nullptr, "AST should not be null");
    ASSERT_EQ(ast->type, ASTNode::NodeType::PROGRAM, "Root should be PROGRAM");
    
    ProgramNode* program = static_cast<ProgramNode*>(ast.get());
    ASSERT_EQ(program->declarations.size(), 1, "Should have 1 statement");
    
    ASTNode* stmt = program->declarations[0].get();
    ASSERT_EQ(stmt->type, ASTNode::NodeType::FUNC_DECL, "Should be FUNC_DECL");
    
    FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(stmt);
    ASSERT_EQ(funcDecl->funcName, "identity", "Function name should be 'identity'");
    ASSERT_EQ(funcDecl->genericParams.size(), 1, "Should have 1 generic parameter");
    ASSERT_EQ(funcDecl->genericParams[0].name, "T", "Generic param should be 'T'");
    ASSERT_EQ(funcDecl->returnType, "*T", "Return type should be '*T'");
    ASSERT_EQ(funcDecl->parameters.size(), 1, "Should have 1 parameter");
    
    ParameterNode* param = static_cast<ParameterNode*>(funcDecl->parameters[0].get());
    ASSERT_EQ(param->typeName, "*T", "Parameter type should be '*T'");
    ASSERT_EQ(param->paramName, "value", "Parameter name should be 'value'");
}

TEST_CASE(parse_generic_function_multiple_params) {
    std::string source = R"(
        func<T, U>:pair = *T(*T:first, *U:second) {
            return first;
        };
    )";
    
    auto ast = parseSource(source);
    ASSERT(ast != nullptr, "AST should not be null");
    
    ProgramNode* program = static_cast<ProgramNode*>(ast.get());
    FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(program->declarations[0].get());
    
    ASSERT_EQ(funcDecl->funcName, "pair", "Function name should be 'pair'");
    ASSERT_EQ(funcDecl->genericParams.size(), 2, "Should have 2 generic parameters");
    ASSERT_EQ(funcDecl->genericParams[0].name, "T", "First param should be 'T'");
    ASSERT_EQ(funcDecl->genericParams[1].name, "U", "Second param should be 'U'");
    ASSERT_EQ(funcDecl->returnType, "*T", "Return type should be '*T'");
    ASSERT_EQ(funcDecl->parameters.size(), 2, "Should have 2 parameters");
    
    ParameterNode* param1 = static_cast<ParameterNode*>(funcDecl->parameters[0].get());
    ASSERT_EQ(param1->typeName, "*T", "First parameter type should be '*T'");
    ASSERT_EQ(param1->paramName, "first", "First parameter name should be 'first'");
    
    ParameterNode* param2 = static_cast<ParameterNode*>(funcDecl->parameters[1].get());
    ASSERT_EQ(param2->typeName, "*U", "Second parameter type should be '*U'");
    ASSERT_EQ(param2->paramName, "second", "Second parameter name should be 'second'");
}

TEST_CASE(parse_generic_function_mixed_types) {
    std::string source = R"(
        func<T>:printValue = bool(*T:value, int32:count) {
            return true;
        };
    )";
    
    auto ast = parseSource(source);
    ASSERT(ast != nullptr, "AST should not be null");
    
    ProgramNode* program = static_cast<ProgramNode*>(ast.get());
    ASSERT_EQ(program->declarations.size(), 1, "Should have 1 declaration");
    ASSERT(program->declarations[0] != nullptr, "First declaration should not be null");
    ASSERT_EQ(program->declarations[0]->type, ASTNode::NodeType::FUNC_DECL, "Should be FUNC_DECL");
    
    FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(program->declarations[0].get());
    ASSERT(funcDecl != nullptr, "Cast to FuncDeclStmt should succeed");
    
    ASSERT_EQ(funcDecl->funcName, "printValue", "Function name should be 'printValue'");
    ASSERT_EQ(funcDecl->genericParams.size(), 1, "Should have 1 generic parameter");
    ASSERT_EQ(funcDecl->returnType, "bool", "Return type should be 'bool'");
    ASSERT_EQ(funcDecl->parameters.size(), 2, "Should have 2 parameters");
    
    ParameterNode* param1 = static_cast<ParameterNode*>(funcDecl->parameters[0].get());
    ASSERT_EQ(param1->typeName, "*T", "First parameter type should be '*T'");
    
    ParameterNode* param2 = static_cast<ParameterNode*>(funcDecl->parameters[1].get());
    ASSERT_EQ(param2->typeName, "int32", "Second parameter type should be 'int32'");
}

TEST_CASE(parse_non_generic_function) {
    std::string source = R"(
        func:add = int32(int32:a, int32:b) {
            return a + b;
        };
    )";
    
    auto ast = parseSource(source);
    ASSERT(ast != nullptr, "AST should not be null");
    
    ProgramNode* program = static_cast<ProgramNode*>(ast.get());
    FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(program->declarations[0].get());
    
    ASSERT_EQ(funcDecl->funcName, "add", "Function name should be 'add'");
    ASSERT_EQ(funcDecl->genericParams.size(), 0, "Should have 0 generic parameters");
    ASSERT_EQ(funcDecl->returnType, "int32", "Return type should be 'int32'");
}

// ----------------------------------------------------------------------------
// Generic Variable Declaration Tests
// ----------------------------------------------------------------------------

TEST_CASE(parse_generic_var_decl) {
    std::string source = R"(
        func<T>:test = int32(*T:param) {
            *T:local = param;
        };
    )";
    
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    
    if (parser.hasErrors()) {
        std::cout << "Parser errors:" << std::endl;
        for (const auto& err : parser.getErrors()) {
            std::cout << "  " << err << std::endl;
        }
    }
    ASSERT(ast != nullptr, "AST should not be null");
    
    ProgramNode* program = static_cast<ProgramNode*>(ast.get());
    FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(program->declarations[0].get());
    
    ASSERT(funcDecl->body != nullptr, "Body should not be null");
    BlockStmt* block = static_cast<BlockStmt*>(funcDecl->body.get());
    ASSERT_EQ(block->statements.size(), 1, "Block should have 1 statement");
    
    VarDeclStmt* varDecl = static_cast<VarDeclStmt*>(block->statements[0].get());
    ASSERT_EQ(varDecl->typeName, "*T", "Variable type should be '*T'");
    ASSERT_EQ(varDecl->varName, "local", "Variable name should be 'local'");
}
// ----------------------------------------------------------------------------
// Generic Parameter Constraint Tests - Phase 3.4 Part 4
// ----------------------------------------------------------------------------

TEST_CASE(parse_generic_function_single_constraint) {
    std::string source = R"(
        func<T: Addable>:add = *T(*T:a, *T:b) {
            return a + b;
        };
    )";
    
    auto ast = parseSource(source);
    ASSERT(ast != nullptr, "AST should not be null");
    
    ProgramNode* program = static_cast<ProgramNode*>(ast.get());
    FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(program->declarations[0].get());
    
    ASSERT_EQ(funcDecl->funcName, "add", "Function name should be 'add'");
    ASSERT_EQ(funcDecl->genericParams.size(), 1, "Should have 1 generic parameter");
    ASSERT_EQ(funcDecl->genericParams[0].name, "T", "Generic param should be 'T'");
    ASSERT_EQ(funcDecl->genericParams[0].constraints.size(), 1, "Should have 1 constraint");
    ASSERT_EQ(funcDecl->genericParams[0].constraints[0], "Addable", "Constraint should be 'Addable'");
}

TEST_CASE(parse_generic_function_multiple_constraints) {
    std::string source = R"(
        func<T: Addable & Display>:printAdd = *T(*T:a, *T:b) {
            return a + b;
        };
    )";
    
    auto ast = parseSource(source);
    ASSERT(ast != nullptr, "AST should not be null");
    
    ProgramNode* program = static_cast<ProgramNode*>(ast.get());
    FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(program->declarations[0].get());
    
    ASSERT_EQ(funcDecl->funcName, "printAdd", "Function name should be 'printAdd'");
    ASSERT_EQ(funcDecl->genericParams.size(), 1, "Should have 1 generic parameter");
    ASSERT_EQ(funcDecl->genericParams[0].name, "T", "Generic param should be 'T'");
    ASSERT_EQ(funcDecl->genericParams[0].constraints.size(), 2, "Should have 2 constraints");
    ASSERT_EQ(funcDecl->genericParams[0].constraints[0], "Addable", "First constraint should be 'Addable'");
    ASSERT_EQ(funcDecl->genericParams[0].constraints[1], "Display", "Second constraint should be 'Display'");
}

TEST_CASE(parse_generic_function_mixed_constraints) {
    std::string source = R"(
        func<T: Hashable & Display, U>:printPair = *T(*T:first, *U:second) {
            return first;
        };
    )";
    
    auto ast = parseSource(source);
    ASSERT(ast != nullptr, "AST should not be null");
    
    ProgramNode* program = static_cast<ProgramNode*>(ast.get());
    FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(program->declarations[0].get());
    
    ASSERT_EQ(funcDecl->funcName, "printPair", "Function name should be 'printPair'");
    ASSERT_EQ(funcDecl->genericParams.size(), 2, "Should have 2 generic parameters");
    
    // First parameter with constraints
    ASSERT_EQ(funcDecl->genericParams[0].name, "T", "First param should be 'T'");
    ASSERT_EQ(funcDecl->genericParams[0].constraints.size(), 2, "T should have 2 constraints");
    ASSERT_EQ(funcDecl->genericParams[0].constraints[0], "Hashable", "First constraint should be 'Hashable'");
    ASSERT_EQ(funcDecl->genericParams[0].constraints[1], "Display", "Second constraint should be 'Display'");
    
    // Second parameter without constraints
    ASSERT_EQ(funcDecl->genericParams[1].name, "U", "Second param should be 'U'");
    ASSERT_EQ(funcDecl->genericParams[1].constraints.size(), 0, "U should have no constraints");
}

// ============================================================================
// Phase 3.4 Part 5: Turbofish Syntax Tests
// ============================================================================

// Test parsing function call with turbofish (explicit type arguments)
TEST_CASE(parse_call_with_turbofish_single_type) {
    std::string source = R"(
        int32:x = identity::<int32>(42);
    )";
    
    auto ast = parseSource(source);
    ASSERT(ast != nullptr, "AST should not be null");
    
    ProgramNode* program = static_cast<ProgramNode*>(ast.get());
    ASSERT_EQ(program->declarations.size(), 1, "Should have 1 declaration");
    
    VarDeclStmt* varDecl = static_cast<VarDeclStmt*>(program->declarations[0].get());
    ASSERT(varDecl->initializer != nullptr, "Variable should have initializer");
    ASSERT_EQ(varDecl->initializer->type, ASTNode::NodeType::CALL, "Initializer should be call expression");
    
    CallExpr* call = static_cast<CallExpr*>(varDecl->initializer.get());
    ASSERT_EQ(call->explicitTypeArgs.size(), 1, "Should have 1 explicit type argument");
    ASSERT_EQ(call->explicitTypeArgs[0], "int32", "Type argument should be 'int32'");
    ASSERT_EQ(call->arguments.size(), 1, "Should have 1 argument");
}

// Test parsing function call with multiple turbofish type arguments
TEST_CASE(parse_call_with_turbofish_multiple_types) {
    std::string source = R"(
        obj:output = convert::<T, U>(value);
    )";
    
    auto ast = parseSource(source);
    ASSERT(ast != nullptr, "AST should not be null");
    
    ProgramNode* program = static_cast<ProgramNode*>(ast.get());
    ASSERT_EQ(program->declarations.size(), 1, "Should have 1 declaration");
    
    VarDeclStmt* varDecl = static_cast<VarDeclStmt*>(program->declarations[0].get());
    ASSERT(varDecl->initializer != nullptr, "Variable should have initializer");
    ASSERT_EQ(varDecl->initializer->type, ASTNode::NodeType::CALL, "Initializer should be call expression");
    
    CallExpr* call = static_cast<CallExpr*>(varDecl->initializer.get());
    
    ASSERT_EQ(call->explicitTypeArgs.size(), 2, "Should have 2 explicit type arguments");
    ASSERT_EQ(call->explicitTypeArgs[0], "T", "First type should be 'T'");
    ASSERT_EQ(call->explicitTypeArgs[1], "U", "Second type should be 'U'");
    ASSERT_EQ(call->arguments.size(), 1, "Should have 1 argument");
}

// Test parsing function call without turbofish (implicit inference)
TEST_CASE(parse_call_without_turbofish) {
    std::string source = R"(
        int32:x = identity(42);
    )";
    
    auto ast = parseSource(source);
    ASSERT(ast != nullptr, "AST should not be null");
    
    ProgramNode* program = static_cast<ProgramNode*>(ast.get());
    VarDeclStmt* varDecl = static_cast<VarDeclStmt*>(program->declarations[0].get());
    CallExpr* call = static_cast<CallExpr*>(varDecl->initializer.get());
    
    ASSERT_EQ(call->explicitTypeArgs.size(), 0, "Should have no explicit type arguments");
    ASSERT_EQ(call->arguments.size(), 1, "Should have 1 argument");
}

// Test parsing turbofish with complex type names
TEST_CASE(parse_call_with_turbofish_complex_types) {
    std::string source = R"(
        obj:map = create::<string, tbb32, bool>(key, value, flag);
    )";
    
    auto ast = parseSource(source);
    ASSERT(ast != nullptr, "AST should not be null");
    
    ProgramNode* program = static_cast<ProgramNode*>(ast.get());
    VarDeclStmt* varDecl = static_cast<VarDeclStmt*>(program->declarations[0].get());
    CallExpr* call = static_cast<CallExpr*>(varDecl->initializer.get());
    
    ASSERT_EQ(call->explicitTypeArgs.size(), 3, "Should have 3 explicit type arguments");
    ASSERT_EQ(call->explicitTypeArgs[0], "string", "First type should be 'string'");
    ASSERT_EQ(call->explicitTypeArgs[1], "tbb32", "Second type should be 'tbb32'");
    ASSERT_EQ(call->explicitTypeArgs[2], "bool", "Third type should be 'bool'");
    ASSERT_EQ(call->arguments.size(), 3, "Should have 3 arguments");
}