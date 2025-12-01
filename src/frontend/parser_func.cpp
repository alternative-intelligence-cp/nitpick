/**
 * src/frontend/parser_func.cpp
 * 
 * Aria Compiler - Function and Lambda Parser
 * Version: 0.0.6
 * 
 * Implements parsing logic for first-class functions, closures, and lambdas.
 * Handles the distinction between named function declarations and anonymous lambdas.
 * 
 * Grammar:
 *   FuncDecl -> "func" ("<" Generics ">")? ":" Identifier "=" "(" Params ")" "->" Type Block
 *   Lambda   -> "(" Params ")" "=>" ( Expr | Block )
 * 
 * Dependencies:
 * - parser.h: Base Parser class
 * - ast/expr.h: FuncExpr, LambdaExpr
 * - ast/decl.h: FuncDecl
 */

#include "parser.h"
#include "ast.h"
#include "tokens.h"
#include <vector>
#include <memory>
#include <string>

// Helper: Parse Parameter List: (type:name, type:name)
std::vector<Param> Parser::parseParams() {
    std::vector<Param> params;
    consume(TOKEN_LEFT_PAREN, "Expected '(' to begin parameter list");
    
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            // Parse Type
            // Note: In Aria, type comes first in params: "int:x", unlike "x int" in Go.
            Token typeTok = consume(TOKEN_TYPE_IDENTIFIER, "Expected parameter type");
            consume(TOKEN_COLON, "Expected ':' after type");
            Token nameTok = consume(TOKEN_IDENTIFIER, "Expected parameter name");
            
            params.push_back({typeTok.lexeme, nameTok.lexeme});
        } while (match(TOKEN_COMMA));
    }
    
    consume(TOKEN_RIGHT_PAREN, "Expected ')' to end parameter list");
    return params;
}

// Parses: func<T>:name = (args) -> ret {... }
std::unique_ptr<FuncDecl> Parser::parseFuncDecl() {
    consume(TOKEN_FUNC, "Expected 'func' keyword");

    // 1. Generics (Optional)
    // Supports: func<T, U>:name
    std::vector<std::string> generics;
    if (match(TOKEN_LESS_THAN)) {
        do {
            Token genType = consume(TOKEN_TYPE_IDENTIFIER, "Expected generic type parameter");
            generics.push_back(genType.lexeme);
        } while (match(TOKEN_COMMA));
        consume(TOKEN_GREATER_THAN, "Expected '>' after generics");
    }

    // 2. Name Binding
    consume(TOKEN_COLON, "Expected ':' before function name");
    Token nameToken = consume(TOKEN_IDENTIFIER, "Expected function name");

    // 3. Assignment (Functions are values in Aria)
    // Note: This syntax "func:name =" emphasizes first-class nature.
    // If no assignment, it is a prototype (e.g. inside interface or extern).
    if (match(TOKEN_ASSIGN)) {
        // Definition follows
    } else {
        // Prototype only (extern or interface)
        consume(TOKEN_SEMICOLON, "Expected ';' or '=' for function");
        // Return declarations with null body
        return std::make_unique<FuncDecl>(nameToken.lexeme, generics, std::vector<Param>(), "", nullptr);
    }

    // 4. Parameters
    std::vector<Param> params = parseParams();

    // 5. Return Type (Optional, default void)
    std::string returnType = "void";
    if (match(TOKEN_ARROW)) { // '->'
        Token retTok = consume(TOKEN_TYPE_IDENTIFIER, "Expected return type");
        returnType = retTok.lexeme;
    }

    // 6. Body
    std::unique_ptr<Block> body = parseBlock();

    // 7. Semicolon (End of statement)
    // In Aria v0.0.6, top-level statements end with semicolons.
    match(TOKEN_SEMICOLON);

    auto decl = std::make_unique<FuncDecl>(nameToken.lexeme, generics, params, returnType, std::move(body));
    
    // Register in Symbol Table
    // We register the function name with a generated signature type string "func"
    // This allows the Borrow Checker to resolve calls to this function later.
    currentScope->define(decl->name, "func"); 

    return decl;
}

// Parses Lambda: (a, b) => expr
// Called by parseAtom or parsePrefix when logic detects lambda syntax.
std::unique_ptr<Expression> Parser::parseLambda() {
    // Note: Parsing lambdas is tricky because they start with '(', same as grouping.
    // The main parseExpression loop usually dispatches this via lookahead.
    
    // 1. Parameters
    // We reuse the parameter parsing logic, though lambdas might support type inference in v0.1.
    // For v0.0.6, explicit types are required in lambdas too.
    std::vector<Param> params = parseParams();
    
    // 2. Arrow Operator
    consume(TOKEN_LAMBDA_ARROW, "Expected '=>' in lambda");
    
    auto lambda = std::make_unique<LambdaExpr>();
    lambda->params = params;
    
    // 3. Body (Block or Expression)
    if (check(TOKEN_LEFT_BRACE)) {
        // Block Body: (args) => {... }
        lambda->bodyBlock = parseBlock();
        lambda->isExpressionBody = false;
    } else {
        // Single expression body: (x) => x + 1
        // Implicit return of the expression value.
        lambda->bodyExpr = parseExpression();
        lambda->isExpressionBody = true;
    }
    
    return lambda;
}
